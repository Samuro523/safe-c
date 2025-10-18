
// guiandroidrender.c

use ../arraye, ../draw3d, ../image, ../queue;

//----------------------------------------------------------------------------------------

struct INFO
{
  uint         seqnr;
  VERTEX_ID    vid;    // 6 vertexes for 2 triangles
  TEXTURE_ID   tid;
}

package AR = new ARRAY_EXTENDER (ELEMENT => INFO);

INFO[]^ g_dialogs;   // ordered from front to rear

//----------------------------------------------------------------------------------------

package J1 = new SAFE_QUEUE (INFO => DIALOG_JOB^);

J1.QUEUE g_job_queue;

//----------------------------------------------------------------------------------------

// called by windows thread

public void insert_dialog_job (DIALOG_JOB job)
{
  DIALOG_JOB^ j = new DIALOG_JOB ' (job);

  J1.enqueue (ref g_job_queue, j);
  
  // tell main app we have work
  if (draw3d.g_dialogs_need_repaint != null)
    draw3d.g_dialogs_need_repaint();
}

//----------------------------------------------------------------------------------------

int index_of (uint seqnr)
{
  int i;

  for (i=0; i<g_dialogs^'length; i++)
  {
    if (seqnr == g_dialogs^[i].seqnr)
      return i;
  }

  return -1;
}

//----------------------------------------------------------------------------------------

// called by draw3d thread

void render_all_dialogs ()
{
  {
    DIALOG_JOB^ pjob;

    while (J1.dequeue (ref g_job_queue, out pjob) == 0)
    {
      {
        ref DIALOG_JOB job = pjob^;

        switch (job.typ)
        {
          case JOB_ADD:
            {
              INFO info;

              clear info;
              info.seqnr = job.seqnr;

              insert (ref g_dialogs, index => 0, element => info);
            }
            break;

          case JOB_UPDATE_ATTR:
            {
              int idx = index_of (job.seqnr);
              if (idx >= 0)
              {
                ref INFO info = g_dialogs^[idx];

                {
                  VERTEX[6] v;
                  uint      color;

                  if (info.vid != 0)
                    free_vertex_buffer (info.vid);

                  color = (((uint)job.transparency) << 24) + 0xFFFFFF;

                  clear v;

//$
// log ("render window at x,y = %d %d  size %d %d", job.x, job.y, job.size_x, job.size_y);
                  
                  v[0] = {c => {(float)job.x, (float)job.y, 0.0},
                          n => {0.0, 1.0, 0.0},
                          u => {0.0, 1.0, 0.0},
                          o => color,
                          t => {0.0, 0.0}};

                  v[1] = v[0];
                  v[1].c[0] += (float)job.size_x;
                  v[1].t[0] = 1.0;

                  v[2] = v[0];
                  v[2].c[1] += (float)job.size_y;  // down
                  v[2].t[1] = 1.0;

                  v[3] = v[1];

                  v[4] = v[1];
                  v[4].c[1] += (float)job.size_y;
                  v[4].t[1] = 1.0;

                  v[5] = v[2];

                  info.vid = create_vertex_buffer (v => v, allow_update => false);
                }
              }
            }
            break;

          case JOB_UPDATE_IMAGE_RECT:
            {
              int idx = index_of (job.seqnr);
              if (idx >= 0)
              {
                ref INFO info = g_dialogs^[idx];

  //$                
                if (info.tid != 0)
                  free_texture (info.tid);

                info.tid = create_texture (job.image,
                                           allow_update => true,
                                           use_mipmapping => false,
                                           rgb_to_linear_conversion => true);

                free_image (ref job.image);
              }
            }
            break;

          case JOB_MOVE_ON_TOP:
            {
              int idx = index_of (job.seqnr);
              if (idx >= 0)
              {
                INFO info = g_dialogs^[idx];
                remove (ref g_dialogs, index => idx);
                insert (ref g_dialogs, index => 0, element => info);
              }
            }
            break;

          case JOB_DELETE:
            {
              int idx = index_of (job.seqnr);
              if (idx >= 0)
              {
                INFO info = g_dialogs^[idx];

                if (info.vid != 0)
                  free_vertex_buffer (info.vid);

                if (info.tid != 0)
                  free_texture (info.tid);

                remove (ref g_dialogs, index => idx);
              }
            }
            break;

          default:
            break;
        }
      }

      free pjob;
    }
  }


  {
    int i;
    for (i=g_dialogs^'length-1; i>=0; i--)   // last to first (bottom to front)
    {
      ref INFO info = g_dialogs^[i];
      if (info.tid != 0)
      {
        draw3d.draw_triangles (v_id      => info.vid,
                               v_offset  => 0,
                               v_count   => 6,
                               t_id      => {info.tid, 0, 0},  // 0:albedo, 1:normal, 2:MRO(R:ambiant occlusion, G:roughness, B:metallic)
                               object_id => 0xFFFFFF);         // user value 0
//$                
      }
    }
  }
}

//----------------------------------------------------------------------------------------

// called by windows thread

public void add_hook_in_draw3d_to_render_dialogs ()
{
  g_dialogs = new INFO[0];
  draw3d.g_func_render_all_dialogs = render_all_dialogs;
}

//----------------------------------------------------------------------------------------
