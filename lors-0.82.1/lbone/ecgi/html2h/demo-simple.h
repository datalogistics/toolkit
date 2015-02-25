/****************************************************
 *       Header file generated with html2h 0.1      *
 *            (c) 2000 by Sven@Dawitz.de            *
 ****************************************************


****************************************
htmlSimpleHead                   "Header of Simple demo file"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------
phone                           text
areaselect                      select

FORMAT     NAME                 COMMENT
----------------------------------------
s          script               "Script Name"
s          name                 "Name of logged in User"
s          phone                "Phone Number"


****************************************
htmlSimpleSelectRow              "The select box items - one row for every possible"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------

FORMAT     NAME                 COMMENT
----------------------------------------
s          selected             "Write SELECTED here, if selected"
30s        katname              "Area Name - bound to right"


****************************************
htmlSimpleSelectBody            
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------

FORMAT     NAME                 COMMENT
----------------------------------------


****************************************
htmlSimplePhoneRow               "This lists all existing entries"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------

FORMAT     NAME                 COMMENT
----------------------------------------
s          number               "Phone Number as string"
s          area                 "Area of number"
           ***SCRIPT LINK***    "Link to Delete Entry"
s               script               "Script Name"
s               skey                 "Session ID"
d               eid                  "ID of the specific entry"
           ***SCRIPT LINK***    "Link to Edit this Entry"
s               script               "Script Name"
s               skey                 "Session ID"
d               eid                 


****************************************
htmlSimpleFoot                  
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------
Submit                          submit
Submit2                         reset

FORMAT     NAME                 COMMENT
----------------------------------------

 ****************************************************/


const char htmlSimpleHead[]="
<html>
<head>
<title>Untitled Document</title>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">
</head>

<body bgcolor=\"#FFFFFF\" text=\"#000000\">
<form name=\"form1\" method=\"post\" action=\"%s\">
  <p>Hello %s! </p>
  <p>Your Phonenumber: 
    <input type=\"text\" name=\"phone\" value=\"%s>
    <br>
    Select Area: 
    <select name=\"areaselect\">

";

const char htmlSimpleSelectRow[]="
      <option %s>%30s</option>


";

const char htmlSimpleSelectBody[]="
    </select>
  </p>
  <p>Current Numbers:</p>
  <table width=\"400\" border=\"0\" cellpadding=\"4\">
    <tr> 
      <td>Number</td>
      <td>Area</td>
      <td> 
        <div align=\"center\">Delete</div>
      </td>
      <td> 
        <div align=\"center\">Edit</div>
      </td>
    </tr>

";

const char htmlSimplePhoneRow[]="
    <tr> 
      <td>%s</td>
      <td>%s</td>
      <td> 
        <div align=\"center\"><a href=\"%s?skey=%s&eid=%d\">-&gt;X&lt;-</a></div>
      </td>
      <td> 
        <div align=\"center\"><a href=\"%s?skey=%s&eid=%d-&gt;X&lt;-</a></div>
      </td>
    </tr>

";

const char htmlSimpleFoot[]="

  </table>
  <p> 
    <input type=\"submit\" name=\"Submit\" value=\"Submit\">
    <input type=\"reset\" name=\"Submit2\" value=\"Reset\">
  </p>
</form>
</body>
</html>
";
