
// xml.h : read XML file

//-------------------------------------------------------------------------
struct XML;
//-------------------------------------------------------------------------

// returns 0 if OK, -1 if error
int open_xml  (out XML xml, string filename);
int wopen_xml (out XML xml, wstring filename);
int close_xml (ref XML xml);

//-------------------------------------------------------------------------

// obtain name of next element
// name'length can be up to 4096
// returns 0 if OK, +1 if no more tag, -1 if syntax error

int get_xml_begin_element (ref XML xml, out string name);

//-------------------------------------------------------------------------

// get next attribute name and value
// name'length and value'length can be up to 4096
// returns 0 if OK, +1 if no more attribute, -1 if syntax error

int get_xml_attribute (ref XML xml, out string name, out string value);

//-------------------------------------------------------------------------

// obtain value of element and close element
// value'length can be up to 16 MB
// returns 0 if OK, +1 if no more tag, -1 if syntax error

int get_xml_end_element (ref XML xml, out string value);

// same but returns value allocated on heap (user must deallocate it), or null in case of error.
string^ get_xml_end_element_value (ref XML xml);

//-------------------------------------------------------------------------

// get message about last xml error

void get_xml_error (XML xml, out string error_message);

//-------------------------------------------------------------------------

#if 0

HOW TO USE
==========

from std use console, xml;

int analyse_tag (ref XML xml)
{
  char  name[4096], value[4096], error_message[100];
  int   rc;

  for (;;)
  {
    rc = get_xml_begin_element (ref xml, out name);
    if (rc < 0)
    {
      get_xml_error (xml, out error_message);
      printf ("error: get_xml_begin_element() : %s\n", error_message);
      return -1;
    }

    if (rc == +1)   // no more tags
      return +1;

    printf ("TAG : %s\n", name);

    for (;;)
    {
      rc = get_xml_attribute (ref xml, out name, out value);
      if (rc == +1)   // no more attributes
        break;

      if (rc < 0)
      {
        get_xml_error (xml, out error_message);
        printf ("error: get_xml_attribute() : %s\n", error_message);
        return -1;
      }

      printf ("ATTRIBUTE (name=%s, value=%s)\n", name, value);
    }


    rc = analyse_tag (ref xml);
    if (rc < 0)
      return -1;


    rc = get_xml_end_element (ref xml, out value);
    if (rc < 0)
    {
      get_xml_error (xml, out error_message);
      printf ("error: get_xml_end_element() : %s\n", error_message);
      return -1;
    }

    printf ("END_TAG (value=%s)\n", value);
  }

  return 0;
}


void main()
{
  XML   xml;
  open_xml (out xml, "test.xml");
  analyse_tag (ref xml);
  close_xml (ref xml);
}

#endif

//-------------------------------------------------------------------------
