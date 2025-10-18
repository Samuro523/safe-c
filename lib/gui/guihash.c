
// guihash.c

use ../gui, ../thread, ../hash;

#if WINDOWS
  use ../win/windows;
#elif ANDROID
  use guitree;  
#endif  

//--------------------------------------------------------------------

bool same_id (DIALOG_ID id1, DIALOG_ID id2);
int hash_of_id (DIALOG_ID id);

struct INFO
{
  int  creation_pending_to_be_deleted;
  bool creation_pending_to_be_created;
  HWND hwnd;   // HWND'max means not active
}

package DL = new HashTable (KEY      => DIALOG_ID,
                            VALUE    => INFO,
                            same_key => same_id,
                            hash_of  => hash_of_id);

SHARED_OBJECT  g_so_dialogs;
bool           g_dialogs_initialized;
DL.HASH_TABLE  g_dialogs;

//--------------------------------------------------------------------

bool same_id (DIALOG_ID id1, DIALOG_ID id2)
{
  return id1 == id2;
}

int hash_of_id (DIALOG_ID id)
{
  return default_hash_of (id);
}

//--------------------------------------------------------------------

void create_it ()
{
  if (!g_dialogs_initialized)
  {
    DL.create_hash_table (out g_dialogs, nb_slots => 4);
    g_dialogs_initialized = true;
  }
}

//--------------------------------------------------------------------

// + do a postmessage create !
public void treat_create_dialog (DIALOG_ID id)
{
  INFO info;

  enter_shared_object (ref g_so_dialogs);
  create_it ();

  if (!find (key => id, out value => info, g_dialogs))
  {
    insert (    key => id,
                value => INFO'{creation_pending_to_be_deleted => 0,
                               creation_pending_to_be_created => true,
                               hwnd                           => HWND'max},
            ref g_dialogs);
  }
  else
  {
    assert !info.creation_pending_to_be_created;
    assert info.hwnd == HWND'max;

    info.creation_pending_to_be_created = true;

    update (key => id, value => info, ref g_dialogs);
  }

  leave_shared_object (ref g_so_dialogs);
}

//--------------------------------------------------------------------

// when returning false, you must DESTROY the window.

public bool treat_init_window (DIALOG_ID id, HWND hwnd)
{
  INFO info;
  bool ret;

  enter_shared_object (ref g_so_dialogs);
  create_it ();

  assert find (key => id, out value => info, g_dialogs);

  if (info.creation_pending_to_be_deleted > 0)   // some deletion calls are pending
  {
    info.creation_pending_to_be_deleted--;
    if (info.creation_pending_to_be_deleted == 0 && !info.creation_pending_to_be_created)
    {
      delete (id, ref g_dialogs);
    }
    else
    {
      update (key => id, value => info, ref g_dialogs);
    }

    ret = false;
  }
  else   // we can actually assign the hwnd, dialog is ready
  {
    info.creation_pending_to_be_created = false;
    info.hwnd = hwnd;

    update (key => id, value => info, ref g_dialogs);

    ret = true;
  }

  leave_shared_object (ref g_so_dialogs);
  return ret;
}

//--------------------------------------------------------------------

// post a destroy message if return value is not zero

public HWND treat_delete_dialog (DIALOG_ID id)
{
  HWND hwnd;
  INFO info;

  enter_shared_object (ref g_so_dialogs);
  create_it ();

  if (find (key => id, out value => info, g_dialogs))
  {
    if (info.creation_pending_to_be_created)
    {
      info.creation_pending_to_be_deleted++;
      info.creation_pending_to_be_created = false;

      update (key => id, value => info, ref g_dialogs);

      hwnd = 0;
    }
    else if (info.hwnd != HWND'max)   // exists
    {
      hwnd = info.hwnd;
      delete (id, ref g_dialogs);
    }
    else
    {
      hwnd = 0;
    }
  }
  else
  {
    hwnd = 0;
  }

  leave_shared_object (ref g_so_dialogs);

  return hwnd;
}

//--------------------------------------------------------------------

// returns 0 if not found

public HWND find_hwnd_of_dialog (DIALOG_ID id)
{
  HWND hwnd;
  INFO info;

  enter_shared_object (ref g_so_dialogs);
  create_it ();

  // returns true if key was found, or false if key does not exist.
  if (find (key => id, out value => info, g_dialogs))
  {
    hwnd = info.hwnd;
    if (hwnd == HWND'max)
      hwnd = 0;
  }
  else
    hwnd = 0;

  leave_shared_object (ref g_so_dialogs);

  return hwnd;
}

//--------------------------------------------------------------------
