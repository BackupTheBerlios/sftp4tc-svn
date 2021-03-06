#include "PreferencesDialog.h"
#include "SessionConfigDialog.h"
#include "wxmyapp.h"
#include "wx/xrc/xmlres.h"          // XRC XML resouces
#include "properties_dlg.h"

//---------------------------------------------------------------------
#define DefaultXrcFileName "rc\\wfx_sftp_cfg.xrc"

//---------------------------------------------------------------------

wxMyApp *app;
wxLog *logStdErr;
char xrcFileName[MAX_PATH];

extern "C" {

void __stdcall InitializeCfgDLL(HMODULE hModule)
{
  GetModuleFileName(hModule, xrcFileName, sizeof(xrcFileName) - 1);
  char *p = strrchr(xrcFileName, '\\');
#ifdef _SFTP_DEBUG
  OutputDebugStringA("InitializeCfgDLL");
  OutputDebugStringA(p);
#endif
  if (p)
    p++;
  else
    p = xrcFileName;
  strcpy(p, DefaultXrcFileName);

//  for(size_t i=0; xrcFileName[i]!=0; i++) if (xrcFileName[i]=='\\') xrcFileName[i]='/';

#ifdef _SFTP_DEBUG
  OutputDebugStringA(xrcFileName);
#endif

  app = new wxMyApp();
#ifdef WXWIN_COMPATIBILITY_2_4
  {
    int argc=0;
    wxChar *argv=NULL;
    app->Initialize(argc, &argv);
  }
#else
  app->Initialize();
#endif

#ifdef _SFTP_DEBUG
  wxLog::SetActiveTarget(new wxLogGui());
#else
  logStdErr = new wxLogStderr();
  wxLog::SetActiveTarget(logStdErr);
#endif
}

//---------------------------------------------------------------------

void __stdcall FreeCfgDLL()
{
  if (app != NULL) {
    app->CleanUp();
    app = NULL;
  }
}

//---------------------------------------------------------------------

bool __stdcall Properties(int Mode, struct config_properties *aProperties)
{
  bool res = false;
  try
  {
    wxXmlResource *xmlres = wxXmlResource::Get();
#ifdef _SFTP_DEBUG
    if (xmlres==NULL)
  	  OutputDebugStringA("wxXmlResource::Get() failed");
#endif
    xmlres->InitAllHandlers();
    bool loadres = xmlres->Load(wxT(xrcFileName));
#ifdef _SFTP_DEBUG
    if (loadres)
  	  OutputDebugStringA("Loaded");
    else
  	  OutputDebugStringA("Not Loaded");
    wxLog::FlushActive();
#endif

    app->SetHWND(aProperties->MainWindow);
    CommonDialog *dlg;
    if (Mode==2)
      dlg = new SessionConfigDialog(aProperties, app->GetHostWindow());
    else
      dlg = new PreferencesDialog(aProperties, app->GetHostWindow());
    
    try
    {
      if (dlg->loaded()) 
      {
        dlg->ShowModal();
        res = true;
      }
    } catch(...) {
    }

    delete dlg;
  } catch(...) {
  }
  app->ReleaseHostWindow();

  return res;
}

}

//---------------------------------------------------------------------

IMPLEMENT_APP_NO_MAIN(wxMyApp);

//---------------------------------------------------------------------

bool wxMyApp::OnInit()
{
  return FALSE;
}

//---------------------------------------------------------------------

void wxMyApp::SetHWND(HWND aMainWindow)
{
  mMainWindow = aMainWindow;
}

//---------------------------------------------------------------------

wxWindow *wxMyApp::GetHostWindow()
{
  if (mMainWindow) {
    mHostWindow = new wxWindow();
    WXHWND hwnd = (WXHWND) mMainWindow;
    mHostWindow->SetHWND(hwnd);
    mHostWindow->Disable();
    return mHostWindow;
  } else {
    mHostWindow = 0;
    return 0;
  }
}

//---------------------------------------------------------------------

void wxMyApp::ReleaseHostWindow()
{
  if (mHostWindow) {
    mHostWindow->Enable();
    mHostWindow->SetHWND(NULL);
    delete mHostWindow;
    mHostWindow = NULL;
  }
}
