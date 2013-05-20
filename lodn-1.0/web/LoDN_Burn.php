<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html><head>
  <meta content="text/html; charset=ISO-8859-1" http-equiv="content-type"><title>LoDN Burning</title></head>

<body>
LoDN Burning<br>
<br>
Welcome to beta LoDN Burning (LoDN-B) page.  LoDN-B allows you to
have
content that is published on LoDN to be burned to CD
or DVD disks.<br>
<br>
This is an experimental service open to educational users at Internet2
connected universities in the US. The burned disks will be mailed via
the US Postal Service using first class mail.<br>
<br>
<span style="font-weight: bold; color: rgb(204, 0, 0);">Every field
must be filled in below in order for us to process your request.</span><br>
<br>
<form method="post" action="burning.php">
  <table style="width: 700px; text-align: left;" border="0" cellpadding="2" cellspacing="0">
    <tbody>
      <tr>
        <td colspan="2" rowspan="1" style="vertical-align: top;"><span style="font-weight: bold;">Contact Info</span><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Name<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input type=\"text\" name="user" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Address1<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="address1" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Address2<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="address2" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">City<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="city" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">State<br>
        </td>
        <td style="vertical-align: top; width: 600px;">
        <select name="state"><option value="al">AL</option><option value="ak">AK</option><option value="ar">AR</option><option value="ca">CA</option><option value="co">CO</option><option value="ct">CT</option><option value="dc">DC</option><option value="de">DE</option><option value="fl">FL</option><option value="ga">GA</option><option value="hi">HI</option><option value="id">ID</option><option value="il">IL</option><option value="in">IN</option><option value="ia">IA</option><option value="ks">KS</option><option value="ky">KY</option><option value="la">LA</option><option value="me">ME</option><option value="md">MD</option><option value="ma">MA</option><option value="mi">MI</option><option value="mn">MN</option><option value="ms">MS</option><option value="mo">MO</option><option value="mt">MT</option><option value="ne">NE</option><option value="nv">NV</option><option value="nh">NH</option><option value="nj">NJ</option><option value="nm">NM</option><option value="ny">NY</option><option value="nc">NC</option><option value="nd">ND</option><option value="oh">OH</option><option value="ok">OK</option><option value="or">OR</option><option value="pa">PA</option><option value="ri">RI</option><option value="sc">SC</option><option value="sd">SD</option><option value="tn" selected="selected">TN</option><option value="tx">TX</option><option value="ut">UT</option><option value="vt">VT</option><option value="va">VA</option><option value="wa">WA</option><option value="wv">WV</option><option value="wi">WI</option><option value="wy">WY</option></select>
        <br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">ZIP<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input maxlength="5" name="zip" size="10"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Email<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="email" size="50"> (.edu addresses only)<br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Phone </td>
        <td style="vertical-align: top; width: 600px;"><input maxlength="12" value="000-00-0000" name="phone" size="15"> </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Position<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="position" value="faculty" checked="checked" type="radio">Faculty   
        <input name="position" value="student" type="radio">Student<br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;">If student, provide this
information as well<br>
        </td>
        <td style="vertical-align: top;">Faculty adviser <input name="adviser" size="35"><br>
Adviser's email <input name="adviser_email" size="35"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">How will
you use this content?<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><textarea rows="10" maxlength="1024" name="usage" cols="50"></textarea> </td>
      </tr>
      <tr>
        <td style="vertical-align: top;">Ship to above address?<br>
        </td>
        <td style="vertical-align: top;"><input name="shipto" value="same" checked="checked" type="radio">Yes     <input name="shipto" value="different" type="radio">No, use the address below<br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Name<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="shipto_user" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Address1<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="shipto_address1" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">Address2<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="shipto_address2" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">City<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input name="shipto_city" size="50"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">State<br>
        </td>
        <td style="vertical-align: top; width: 600px;">
        <select name="shipto_state"><option value="al">AL</option><option value="ak">AK</option><option value="ar">AR</option><option value="ca">CA</option><option value="co">CO</option><option value="ct">CT</option><option value="dc">DC</option><option value="de">DE</option><option value="fl">FL</option><option value="ga">GA</option><option value="hi">HI</option><option value="id">ID</option><option value="il">IL</option><option value="in">IN</option><option value="ia">IA</option><option value="ks">KS</option><option value="ky">KY</option><option value="la">LA</option><option value="me">ME</option><option value="md">MD</option><option value="ma">MA</option><option value="mi">MI</option><option value="mn">MN</option><option value="ms">MS</option><option value="mo">MO</option><option value="mt">MT</option><option value="ne">NE</option><option value="nv">NV</option><option value="nh">NH</option><option value="nj">NJ</option><option value="nm">NM</option><option value="ny">NY</option><option value="nc">NC</option><option value="nd">ND</option><option value="oh">OH</option><option value="ok">OK</option><option value="or">OR</option><option value="pa">PA</option><option value="ri">RI</option><option value="sc">SC</option><option value="sd">SD</option><option value="tn" selected="selected">TN</option><option value="tx">TX</option><option value="ut">UT</option><option value="vt">VT</option><option value="va">VA</option><option value="wa">WA</option><option value="wv">WV</option><option value="wi">WI</option><option value="wy">WY</option></select>
        <br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top; width: 100px;">ZIP<br>
        </td>
        <td style="vertical-align: top; width: 600px;"><input maxlength="5" name="shipto_zip" size="10"><br>
        </td>
      </tr>
      <tr>
        <td colspan="2" rowspan="1" style="vertical-align: top;"><br>
        <span style="font-weight: bold;">Burning Options</span><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;">Content<br>
        </td>
        <td style="vertical-align: top;"><textarea rows="5" maxlength="4096" name="urls" cols="50"><?php echo $_GET['file']; ?>Insert LoDN content exNode URLs here
	</textarea><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;">Media<br>
        </td>
        <td style="vertical-align: top;"><input name="media" value="cd" checked="checked" type="radio">CD     <input name="media" value="dvd" type="radio">DVD<br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;"><br>
        </td>
        <td style="vertical-align: top;"><br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;">User Responsibility<br>
        </td>
        <td style="vertical-align: top;">I take full responsibility for
the content that I have requested and that I have full rights to access
the content. If I suspect that the content is copyrighted and if I
believe that the LoDN user who published it may not have the proper
copyrights, I will notify the LoDN maintainers immediately.<br>
        <input name="responsible" value="agree" type="radio">Agree     <input name="responsible" value="disagree" checked="checked" type="radio">Disagree<br>
        </td>
      </tr>
      <tr>
        <td style="vertical-align: top;"><br>
        </td>
        <td style="vertical-align: top;"><input value="Submit" type="submit">               <input value="Reset" type="reset"></td>
      </tr>
    </tbody>
  </table>
</form>
<br>
<br>
ÿ
</body></html>
