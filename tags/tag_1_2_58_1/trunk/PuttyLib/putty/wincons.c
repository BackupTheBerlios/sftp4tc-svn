/*
 * console.c: various interactive-prompt routines shared between
 * the Windows console PuTTY tools
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "putty.h"
#include "storage.h"
#include "ssh.h"

#include <windows.h>

int console_batch_mode = FALSE;

static void *console_logctx = NULL;
extern Config cfg;

/*
 * Clean up and exit.
 */
void cleanup_exit(int code)
{
    /*
     * Clean up.
     */
    sk_cleanup();

    //WSACleanup(); //hangs sometime !!!

    if (cfg.protocol == PROT_SSH) {

	random_save_seed();
#ifdef MSCRYPTOAPI
	crypto_wrapup();
#endif

    }
}

int verify_ssh_host_key(void *frontend, char *host, int port, char *keytype,
			 char *keystr, char *fingerprint,
       void (*callback)(void *ctx, int result), void *ctx)
{
    int ret;
    HANDLE hin;
    DWORD savemode, i;

    static const char absentmsg_batch[] =
	"The server's host key is not cached in the registry. You\n"
	"have no guarantee that the server is the computer you\n"
	"think it is.\n"
	"The server's %s key fingerprint is:\n"
	"%s\n"
	"Connection abandoned.\n";

    static const char absentmsg[] =
	"The server's host key is not cached in the registry. You\n"
	"have no guarantee that the server is the computer you\n"
	"think it is.\n"
	"The server's key fingerprint is:\n"
	"%s\n\n"
	"If you trust this host, click \"Yes\" to add the key to\n"
	"PuTTY's cache and carry on connecting.\n"
	"If you want to carry on connecting just once, without\n"
	"adding the key to the cache, click \"No\".\n"
	"If you do not trust this host, click \"Cancel\" or hit ESC to abandon the connection.\n"
	"\n"
	"Store key in cache? (Yes/No) \n"
	"Cancel Connection? (Cancel or ESC) ";

    static const char wrongmsg_batch[] =
	"WARNING - POTENTIAL SECURITY BREACH!\n"
	"The server's host key does not match the one PuTTY has\n"
	"cached in the registry. This means that either the\n"
	"server administrator has changed the host key, or you\n"
	"have actually connected to another computer pretending\n"
	"to be the server.\n"
	"The new %s key fingerprint is:\n"
	"%s\n"
	"Connection abandoned.\n";

    static const char wrongmsg[] =
	"WARNING - POTENTIAL SECURITY BREACH!\n"
	"The server's host key does not match the one PuTTY has\n"
	"cached in the registry. This means that either the\n"
	"server administrator has changed the host key, or you\n"
	"have actually connected to another computer pretending\n"
	"to be the server.\n"
	"The new key fingerprint is:\n"
	"%s\n\n"
	"If you were expecting this change and trust the new key,\n"
	"click \"Yes\" to update PuTTY's cache and continue connecting.\n"
	"If you want to carry on connecting but without updating\n"
	"the cache, click \"No\".\n"
	"If you want to abandon the connection completely, click \"Cancel\" or hit ESC.\n"
	"Canceling is the ONLY guaranteed safe choice\n"
	"\n"
	"Update cached key? (Yes/No) \n"
	"Cancel Connection? (Cancel or ESC) ";

    static const char abandoned[] = "Connection abandoned.\n";

    char line[32];
    int msgBoxRet;
    char buf1[10000];		// :-|

    /*
     * Verify the key against the registry.
     */
    ret = verify_host_key(host, port, keytype, keystr);

    if (ret == 0)		       /* success - key matched OK */
	return 1;

    if (ret == 2) {		       /* key was different */
	if (console_batch_mode) {
	    fprintf(stderr, wrongmsg_batch, keytype, fingerprint);
	    cleanup_exit(1);
	}

	sprintf(buf1,wrongmsg,fingerprint);
	msgBoxRet=MessageBox(
	    NULL,							// handle of owner window
	    buf1,							// address of text in message box
	    "PuTTY Security Alert (TC sftp Plugin)",			// address of title of message box
	    MB_YESNOCANCEL|MB_DEFBUTTON3|MB_TASKMODAL|MB_TOPMOST	// style of message box
	);

	fprintf(stderr, wrongmsg, keytype, fingerprint);
	fflush(stderr);
    }
    if (ret == 1) {		       /* key was absent */
	if (console_batch_mode) {
	    fprintf(stderr, absentmsg_batch, keytype, fingerprint);
	    cleanup_exit(1);
	}

	sprintf(buf1,absentmsg,fingerprint);
	msgBoxRet=MessageBox(
	    NULL,							// handle of owner window
	    buf1,							// address of text in message box
	    "PuTTY Security Alert (TC sftp Plugin)",			// address of title of message box
	    MB_YESNOCANCEL|MB_DEFBUTTON3|MB_TASKMODAL|MB_TOPMOST	// style of message box
	);
	fprintf(stderr, absentmsg, keytype, fingerprint);
	fflush(stderr);
    }

    hin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hin, &savemode);
    SetConsoleMode(hin, (savemode | ENABLE_ECHO_INPUT |
			 ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT));
    ReadFile(hin, line, sizeof(line) - 1, &i, NULL);
    SetConsoleMode(hin, savemode);

    if (msgBoxRet == IDYES) {
	store_host_key(host, port, keytype, keystr);
	return 1;
    }

    if (msgBoxRet == IDNO) {
	return 1;
    }
	
    return 0;//ansonsten Abruch 

    if (line[0] != '\0' && line[0] != '\r' && line[0] != '\n') {
	if (line[0] == 'y' || line[0] == 'Y')
	    store_host_key(host, port, keytype, keystr);
    } else {
	fprintf(stderr, abandoned);
	cleanup_exit(0);
    }
}

void update_specials_menu(void *frontend)
{
}

/*
 * Ask whether the selected cipher is acceptable (since it was
 * below the configured 'warn' threshold).
 * cs: 0 = both ways, 1 = client->server, 2 = server->client
 */
void askcipher(void *frontend, char *ciphername, int cs)
{
    HANDLE hin;
    DWORD savemode, i;

    static const char msg[] =
	"The first %scipher supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Continue with connection? (y/n) ";
    static const char msg_batch[] =
	"The first %scipher supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Connection abandoned.\n";
    static const char abandoned[] = "Connection abandoned.\n";

    char line[32];

    if (console_batch_mode) {
	fprintf(stderr, msg_batch,
		(cs == 0) ? "" :
		(cs == 1) ? "client-to-server " : "server-to-client ",
		ciphername);
	cleanup_exit(1);
    }

    fprintf(stderr, msg,
	    (cs == 0) ? "" :
	    (cs == 1) ? "client-to-server " : "server-to-client ",
	    ciphername);
    fflush(stderr);

    hin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hin, &savemode);
    SetConsoleMode(hin, (savemode | ENABLE_ECHO_INPUT |
			 ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT));
    ReadFile(hin, line, sizeof(line) - 1, &i, NULL);
    SetConsoleMode(hin, savemode);

    if (line[0] == 'y' || line[0] == 'Y') {
	return;
    } else {
	fprintf(stderr, abandoned);
	cleanup_exit(0);
    }
}

void notify_remote_exit(void *frontend)
{
}

void set_busy_status(void *frontend, int status)
{
}

void timer_change_notify(long next)
{
}

/*
 * Ask whether the selected algorithm is acceptable (since it was
 * below the configured 'warn' threshold).
 */
int askalg(void *frontend, const char *algtype, const char *algname,
	   void (*callback)(void *ctx, int result), void *ctx)
{
    HANDLE hin;
    DWORD savemode, i;

    static const char msg[] =
	"The first %s supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Continue with connection? (y/n) ";
    static const char msg_batch[] =
	"The first %s supported by the server is\n"
	"%s, which is below the configured warning threshold.\n"
	"Connection abandoned.\n";
    static const char abandoned[] = "Connection abandoned.\n";

    char line[32];

    if (console_batch_mode) {
	fprintf(stderr, msg_batch, algtype, algname);
	return 0;
    }

    fprintf(stderr, msg, algtype, algname);
    fflush(stderr);

    hin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hin, &savemode);
    SetConsoleMode(hin, (savemode | ENABLE_ECHO_INPUT |
			 ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT));
    ReadFile(hin, line, sizeof(line) - 1, &i, NULL);
    SetConsoleMode(hin, savemode);

    if (line[0] == 'y' || line[0] == 'Y') {
	return 1;
    } else {
	fprintf(stderr, abandoned);
	return 0;
    }
}

/*
 * Ask whether to wipe a session log file before writing to it.
 * Returns 2 for wipe, 1 for append, 0 for cancel (don't log).
 */
int askappend(void *frontend, Filename filename,
              void (*callback)(void *ctx, int result), void *ctx)
{
    HANDLE hin;
    DWORD savemode, i;

    static const char msgtemplate[] =
	"The session log file \"%.*s\" already exists.\n"
	"You can overwrite it with a new session log,\n"
	"append your session log to the end of it,\n"
	"or disable session logging for this session.\n"
	"Enter \"y\" to wipe the file, \"n\" to append to it,\n"
	"or just press Return to disable logging.\n"
	"Wipe the log file? (y/n, Return cancels logging) ";

    static const char msgtemplate_batch[] =
	"The session log file \"%.*s\" already exists.\n"
	"Logging will not be enabled.\n";

    char line[32];

    if (console_batch_mode) {
	fprintf(stderr, msgtemplate_batch, FILENAME_MAX, filename.path);
	fflush(stderr);
	return 0;
    }
    fprintf(stderr, msgtemplate, FILENAME_MAX, filename.path);
    fflush(stderr);

    hin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hin, &savemode);
    SetConsoleMode(hin, (savemode | ENABLE_ECHO_INPUT |
			 ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT));
    ReadFile(hin, line, sizeof(line) - 1, &i, NULL);
    SetConsoleMode(hin, savemode);

    if (line[0] == 'y' || line[0] == 'Y')
	return 2;
    else if (line[0] == 'n' || line[0] == 'N')
	return 1;
    else
	return 0;
}

/*
 * Warn about the obsolescent key file format.
 * 
 * Uniquely among these functions, this one does _not_ expect a
 * frontend handle. This means that if PuTTY is ported to a
 * platform which requires frontend handles, this function will be
 * an anomaly. Fortunately, the problem it addresses will not have
 * been present on that platform, so it can plausibly be
 * implemented as an empty function.
 */
void old_keyfile_warning(void)
{
    static const char message[] =
	"You are loading an SSH 2 private key which has an\n"
	"old version of the file format. This means your key\n"
	"file is not fully tamperproof. Future versions of\n"
	"PuTTY may stop supporting this private key format,\n"
	"so we recommend you convert your key to the new\n"
	"format.\n"
	"\n"
	"Once the key is loaded into PuTTYgen, you can perform\n"
	"this conversion simply by saving it again.\n";

    fputs(message, stderr);
}

void console_provide_logctx(void *logctx)
{
    console_logctx = logctx;
}

void logevent(void *frontend, const char *string)
{
    if (console_logctx)
	log_eventlog(console_logctx, string);
}

char *console_password = NULL;

int console_get_line(const char *prompt, char *str,
			    int maxlen, int is_pw)
{
    HANDLE hin, hout;
    DWORD savemode, newmode, i;

    if (is_pw && console_password) {
		
	static int tried_once = 0;

	if (tried_once) {
	    return 0;
	} else {
	    strncpy(str, console_password, maxlen);
	    str[maxlen - 1] = '\0';
	    tried_once = 1;
	    return 1;
	}
    }

    if (console_batch_mode) {
	if (maxlen > 0)
	    str[0] = '\0';
    } else {
	hin = GetStdHandle(STD_INPUT_HANDLE);
	hout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hin == INVALID_HANDLE_VALUE || hout == INVALID_HANDLE_VALUE) {
	    fprintf(stderr, "Cannot get standard input/output handles\n");
	    cleanup_exit(1);
	}

	GetConsoleMode(hin, &savemode);
	newmode = savemode | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT;
	if (is_pw)
	    newmode &= ~ENABLE_ECHO_INPUT;
	else
	    newmode |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hin, newmode);

	WriteFile(hout, prompt, strlen(prompt), &i, NULL);
	ReadFile(hin, str, maxlen - 1, &i, NULL);

	SetConsoleMode(hin, savemode);

	if ((int) i > maxlen)
	    i = maxlen - 1;
	else
	    i = i - 2;
	str[i] = '\0';

	if (is_pw)
	    WriteFile(hout, "\r\n", 2, &i, NULL);

    }
    return 1;
}

void frontend_keypress(void *handle)
{
    /*
     * This is nothing but a stub, in console code.
     */
    return;
}
