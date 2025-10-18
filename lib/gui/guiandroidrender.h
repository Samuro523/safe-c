
// guiandroidrender.h

use ../image;

enum DIALOG_JOB_TYPE {JOB_ADD, JOB_UPDATE_ATTR, JOB_UPDATE_IMAGE_RECT, JOB_MOVE_ON_TOP, JOB_DELETE};

struct DIALOG_JOB (DIALOG_JOB_TYPE typ)
{
  uint seqnr;
  
  switch (typ)
  {
    case JOB_ADD:
      null;
      
    case JOB_UPDATE_ATTR:
      int x, y, size_x, size_y;
      int transparency;   // 255 = full visible, 165 = half visible
      
    case JOB_UPDATE_IMAGE_RECT:
      int         ofs_x, ofs_y;
      IMAGE_INFO  image;     // image.pixel must be freed by the consumer

    case JOB_MOVE_ON_TOP:
      null;

    case JOB_DELETE:
      null;
  }
}

void insert_dialog_job (DIALOG_JOB job);

void add_hook_in_draw3d_to_render_dialogs ();
