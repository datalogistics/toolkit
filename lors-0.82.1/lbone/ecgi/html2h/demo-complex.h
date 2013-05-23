/****************************************************
 *       Header file generated with html2h 0.1      *
 *            (c) 2000 by Sven@Dawitz.de            *
 ****************************************************


****************************************
htmlNewEntryHead                 "Header"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------
testonly                        hidden
testchar                        hidden
kat                             select

FORMAT     NAME                 COMMENT
----------------------------------------
s          script               "Script Name"
10.3f      test                 "Only for testing stuff"
c          testc                "Only for testing a char"
           ***SCRIPT LINK***    "Link to entrylist"
s               script               "Script Name"
d               act                  "Action: List"
s               skey                 "Session ID"
           ***SCRIPT LINK***    "Link to preview"
s               script               "Script Name"
d               act                  "Action: Preview"
s               skey                 "Session ID"
           ***SCRIPT LINK***    "Link to new entry"
s               script               "Script Name"
d               act                  "Action: New Entry"
s               skey                 "Session ID"
           ***SCRIPT LINK***   
s               script               "Script Name"
d               act                  "Action: Kategorien"
s               skey                 "Session ID"
           ***SCRIPT LINK***    "Link to logout"
s               script               "Script Name"
d               act                  "Action: Logout"
s               skey                 "Session ID"


****************************************
htmlNewEntryKatRow               "The select box items - one row for every possible"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------

FORMAT     NAME                 COMMENT
----------------------------------------
s          selected             "Write SELECTED here, if selected"
-20s       katname              "Kategory name"


****************************************
htmlNewEntryNextHead            
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------
title                           textarea
text                            textarea
valid_from                      text
valid_till                      text

FORMAT     NAME                 COMMENT
----------------------------------------
-10s       title                "Title of Entry"
s          text                 "Text of Entry"
s          valid_from           "Valid From"
s          valid_till           "Valid until"


****************************************
htmlNewEntryPicRow               "a Row for every Picture attached"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------

FORMAT     NAME                 COMMENT
----------------------------------------
s          filename            
s          Description         
           ***SCRIPT LINK***    "Link to del a picture"
s               script               "Script Name"
s               bibiid              
d               act                  "Action: Pic Del"
s               skey                 "Session ID"
           ***SCRIPT LINK***    "Link to view a pic"
s               script               "Script Name"
s               bibiid              
d               act                  "Action: Pic View"
s               skey                 "Session ID"


****************************************
htmlNewEntryFoot                 "simple foot of file without params"
****************************************

FORM-INPUT-NAME                 FORM-INPUT-TYPE
----------------------------------------
Submit                          submit
Submit                          submit
reset                           reset

FORMAT     NAME                 COMMENT
----------------------------------------
           ***SCRIPT LINK***   
s               script               "Script Name"
s               skey                 "Session ID"
d               act                  "Action: new Pic"
s               enenid               "Entry ID"

 ****************************************************/


const char htmlNewEntryHead[]="
<html>
<head>
<title>Untitled Document</title>
<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">
<style>
.DataFields { font-family: Verdana, Arial, Helvetica, sans-serif; background-color: #000000; list-style-type: upper-roman; font-size: 11px; font-style: normal; color: #00FF00 ; border-style: groove}
.Text {  font-family: Verdana, Arial, Helvetica, sans-serif; font-size: 12px; color: #000000}
.Table {  background-color: #CCCCCC; border-style: groove; font-family: Verdana, Arial, Helvetica, sans-serif; font-size: 12px}
body {  background-color: #666666}
.button {  font-family: Verdana, Arial, Helvetica, sans-serif; font-size: 11px; font-style: normal; background-color: #CCCCCC; border-style: ridge}
a {  color: #333333}
.TableHead { background-color: #999999; border-style: groove ; font-family: Verdana, Arial, Helvetica, sans-serif; font-size: 12px; font-weight: bold; text-transform: capitalize; text-align: center}
.Tablerow {  background-color: #dcdcdc}
</style>
</head>

<body bgcolor=\"#999999\" text=\"#000000\">
<form name=\"form1\" method=\"post\" action=\"%s\">
<input type=\"hidden\" value=\"%10.3f\" name=\"testonly>
<input type=hidden name=\"testchar\" value=\"%c\">
<table width=\"100%%\" class=\"Table\">
  <tr> 
    <td colspan=\"5\"> 
      <div align=\"center\"><font size=\"5\"><span class=\"Text\"><font size=\"4\">Aktuell 
        Tool V2.0</font></span></font><span class=\"Text\"><br>
        (c) 2000 by <a href=\"http://www.global-owl.de\">Global-OWL GmbH</a> &amp; 
        Sven Dawitz</span></div>
    </td>
  </tr>
  <tr> 
    <td class=\"Text\"> 
      <p align=\"center\"><a href=\"%s?act=%d&skey=%s\">&Uuml;bersicht</a></p>
    </td>
    <td class=\"Text\"> 
      <div align=\"center\"><a href=\"%s?act=%d&skey=%s\">Vorschau</a></div>
    </td>
    <td class=\"Text\"> 
      <div align=\"center\"><a href=\"%s?act=%d&skey=%s\">Neuer Eintrag</a></div>
    </td>
    <td class=\"Text\">
      <div align=\"center\"><a href=\"%s?act=%d&skey=%s\">Kategorien</a></div>
    </td>
    <td class=\"Text\"> 
      <div align=\"center\"><a href=\"%s?act=%d&skey=%s\">Logout</a></div>
    </td>
  </tr>
</table>
<p>&nbsp;</p>
  <table width=\"100%%\" class=\"Table\">
    <tr> 
      <td class=\"TableHead\">Kategorie</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <select name=\"kat\" class=\"DataFields\">

";

const char htmlNewEntryKatRow[]="
          <option %s>%-20s</option>

";

const char htmlNewEntryNextHead[]="

        </select>
      </td>
    </tr>
    <tr> 
      <td class=\"TableHead\">Titel</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <textarea name=\"title\" class=\"DataFields\" cols=\"50\" rows=\"3\">%-10s</textarea>
      </td>
    </tr>
    <tr> 
      <td class=\"TableHead\">Text</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <textarea name=\"text\" cols=\"50\" rows=\"5\" class=\"DataFields\">%s</textarea>
      </td>
    </tr>
    <tr> 
      <td class=\"TableHead\">G&uuml;ltig ab</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <input type=\"text\" name=\"valid_from\" class=\"DataFields\" value=\"%s\" size=\"50\">
      </td>
    </tr>
    <tr> 
      <td class=\"TableHead\">G&uuml;ltig bis</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <input type=\"text\" name=\"valid_till\" class=\"DataFields\" value=\"%s\" size=\"50\">
      </td>
    </tr>
    <tr> 
      <td class=\"TableHead\">Bilder</td>
      <td> 
        <div align=\"center\">:</div>
      </td>
      <td> 
        <table width=\"100%%\" class=\"Table\">
          <tr> 
            <td class=\"TableHead\">Dateiname</td>
            <td class=\"TableHead\">Beschreibung</td>
            <td class=\"TableHead\">l&ouml;schen</td>
            <td class=\"TableHead\">ansehen</td>
          </tr>

";

const char htmlNewEntryPicRow[]="
          <tr> 
            <td class=\"Tablerow\">%s</td>
            <td class=\"Tablerow\">%s</td>
            <td class=\"Tablerow\"> 
              <div align=\"center\"><a href=\"%s?bibiid=%s&act=%d&skey=%s\">-&gt;X&lt;-</a></div>
            </td>
            <td class=\"Tablerow\"> 
              <div align=\"center\"><a href=\"%s?bibiid=%s&act=%d&skey=%s\">-&gt;X&lt;-</a></div>
            </td>
          </tr>


";

const char htmlNewEntryFoot[]="
          <tr> 
            <td class=\"Tablerow\" colspan=\"4\">
              <div align=\"center\"><a href=\"%s?skey=%s&act=%d&enenid=%s\">neues Bild einf&uuml;gen</a></div>
            </td>
          </tr>
        </table>
      </td>
    </tr>
    <tr> 
      <td>&nbsp;</td>
      <td>&nbsp;</td>
      <td>&nbsp;</td>
    </tr>
    <tr> 
      <td colspan=\"3\" class=\"button\"> 
        <div align=\"center\"> 
          <input type=\"submit\" name=\"Submit\" value=\"Speichern\" class=\"button\">
          <input type=\"submit\" name=\"Submit\" value=\"Vorschau ansehen\" class=\"button\">
          <input type=\"reset\" name=\"reset\" value=\"Zur&uuml;cksetzen\" class=\"button\">
        </div>
      </td>
    </tr>
  </table>
</form>
<p>&nbsp;</p>
</body>
</html>
";
