
// directxdef.c

use exception, strings, win/windows;

//---------------------------------------------------------------------
#begin unsafe
//---------------------------------------------------------------------

string^ new_zstring (string s)
{
  string^ p = new string (strlen(s) + 1);
  strcpy (out p^, s);
  return p;
}

//---------------------------------------------------------------------

void message_box (string title, string message)
{
  string^  title2   = new_zstring (title);
  string^  message2 = new_zstring (message);

  MessageBoxA (windows.main_hWnd, &message2^, &title2^, MB_OK);

  free title2;
  free message2;
}

//---------------------------------------------------------------------

void log_report_in_crash_report (string line)
{
  exception.log_in_crash_report ("DirectX : %s\n", line);
}

//---------------------------------------------------------------------

void error_handling (string message, HRESULT error, bool fatal)
{
  char[256] buffer;
  HRESULT   result;
  const string TITLE = "DirectX - graphic driver crash";

  log_report_in_crash_report (TITLE);

  switch ((uint)error)
  {
    case 0x887a0005:
      result = DX.m_device->lpVtbl->GetDeviceRemovedReason (DX.m_device);
      switch ((uint)result)
      {
        case 0x887A0006:
          sprintf (out buffer, "%s 0x%x with DXGI_ERROR_DEVICE_HUNG", message, error);
          if (fatal)
          {
            message_box (TITLE, buffer);
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;

        case 0x887A0005:
          sprintf (out buffer, "%s 0x%x with DXGI_ERROR_DEVICE_REMOVED", message, error);
          if (fatal)
          {
            message_box (TITLE, buffer);
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;

        case 0x887A0007:
          sprintf (out buffer, "%s 0x%x with DXGI_ERROR_DEVICE_RESET", message, error);
          if (fatal)
          {
            message_box (TITLE, buffer);
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;

        case 0x887A0020:
          sprintf (out buffer, "%s 0x%x with DXGI_ERROR_DRIVER_INTERNAL_ERROR", message, error);
          if (fatal)
          {
            message_box (TITLE, buffer);
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;

        case 0x887A0001:
          sprintf (out buffer, "%s 0x%x with DXGI_ERROR_INVALID_CALL", message, error);
          if (fatal)
          {
            message_box (TITLE, buffer);
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;

        default:
          sprintf (out buffer, "%s 0x%x with 0x%x", message, error, result);
          if (fatal)
          {
            message_box (TITLE, buffer);
            assert (result & result) > 0;  // makes sure result code is in EAX for exception
            abort;
          }
          else
          {
            log_report_in_crash_report (buffer);
          }
          break;
      }
      break;

    default:
      sprintf (out buffer, "%s 0x%x", message, error);
      if (fatal)
      {
        message_box (TITLE, buffer);
        assert (error & error) > 0;  // makes sure error code is in EAX for exception
        abort;
      }
      else
      {
        log_report_in_crash_report (buffer);
      }
      break;
  }
}

//---------------------------------------------------------------------

public
void fatal_abort (string message, HRESULT error)
{
  error_handling (message, error, fatal => true);
}

//---------------------------------------------------------------------

public
void log_error (string message, HRESULT error)
{
  error_handling (message, error, fatal => false);
}

//---------------------------------------------------------------------
#end unsafe
//---------------------------------------------------------------------
