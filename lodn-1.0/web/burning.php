<?php
print "<html><head></head><body>";


if($HTTP_POST_VARS['user'] == NULL) {
  echo "You must enter a contact name<br>";
  $error=1;
}
if($HTTP_POST_VARS['address1'] == NULL) {
  echo "You must enter a contact address<br>";
  $error=1;
}
if($HTTP_POST_VARS['city'] == NULL) {
  echo "You must enter a contact city<br>";
  $error=1;
}
if($HTTP_POST_VARS['state'] == NULL) {
  echo "You must enter a contact city<br>";
  $error=1;
}
if($HTTP_POST_VARS['zip'] == NULL) {
  echo "You must enter a contact zipcode<br>";
  $error=1;
}
if($HTTP_POST_VARS['email'] == NULL) {
  echo "You must enter a contact email<br>";
  $error=1;
}
if(!eregi(".edu", $HTTP_POST_VARS['email'])) {
  echo "You must enter a valid .EDU email address<br>";
  $error=1;
}
if($HTTP_POST_VARS['phone'] == NULL ||
   $HTTP_POST_VARS['phone'] == "000-00-0000") {
  echo "You must enter a phone number<br>";
  $error=1;
}
if($HTTP_POST_VARS['position'] == "student") {
  if($HTTP_POST_VARS['adviser'] == NULL) {
    echo "Students must provide an adviser's name<br>";
    $error=1;
  }
  if($HTTP_POST_VARS['adviser_email'] == NULL) {
    echo "Students must provide an adviser's email<br>";
    $error=1;
  }
}
if($HTTP_POST_VARS['usage'] == NULL) {
  echo "Please provide your planned use of the file<br>";
  $error=1;
}
if($HTTP_POST_VARS['shipto'] == "different") {
  if($HTTP_POST_VARS['shipto_user'] == NULL) {
    echo "You must enter a shipping name<br>";
    $error=1;
  }
  if($HTTP_POST_VARS['shipto_address1'] == NULL) {
    echo "You must enter a shipping address<br>";
    $error=1;
  }
  if($HTTP_POST_VARS['shipto_city'] == NULL) {
    echo "You must enter a shipping city<br>";
    $error=1;
  }
  if($HTTP_POST_VARS['shipto_state'] == NULL) {
    echo "You must enter a shipping city<br>";
    $error=1;
  }
  if($HTTP_POST_VARS['shipto_zip'] == NULL) {
    echo "You must enter a shipping zipcode<br>";
    $error=1;
  }
}
if($HTTP_POST_VARS['urls'] == NULL) {
  echo "You must enter content to be mailed!<br>";
  $error=1;
}
if($HTTP_POST_VARS['responsible'] != "agree") {
  echo "You must agree to the terms stated in order to recieve materials<br>";
  $error=1;
}


if($error == 0) {
$MESSAGE = "";
foreach ($HTTP_POST_VARS as $key => $value) 
if($value != NULL) {
  $MESSAGE .= "$key = $value\n";
}

$mailheaders = "From: ".$HTTP_POST_VARS['email'];
mail("atchley@cs.utk.edu", "LoDN Burning Request", $MESSAGE, $mailheaders);

echo "The following information has been mailed to Scott Atchley (atchley@cs.utk.edu)<br>"
    ."<b>Mailing address:</b> <br>"
    ."".$HTTP_POST_VARS['user']."<br>"
    ."".$HTTP_POST_VARS['address1']."<br>";
    if($HTTP_POST_VARS['address2']!=NULL)
echo $HTTP_POST_VARS['address2'];
echo $HTTP_POST_VARS['city'].", ".$HTTP_POST_VARS['state']." ".$HTTP_POST_VARS['zip']."<br>";
echo "<b>Email:</b> ".$HTTP_POST_VARS['email']."<br>";
echo "<b>Phone Number:</b> ".$HTTP_POST_VARS['phone']."<br>";
echo "<b>Position:</b> ".$HTTP_POST_VARS['position']."<br>";
if($HTTP_POST_VARS['position'] == "student") {
echo "<b>Adviser:</b> ".$HTTP_POST_VARS['adviser']."<br>";
echo "<b>Adviser email:</b> ".$HTTP_POST_VARS['adviser_email']."<br>";
}
echo "<b>Usage: </b>".$HTTP_POST_VARS['usage']."<br>";
if($HTTP_POST_VARS['shipto'] != "same") {
    echo "<b>Address to ship to:</b><br>";
    echo $HTTP_POST_VARS['shipto_user']."<br>"
    ."".$HTTP_POST_VARS['shipto_address1']."<br>";
    if($HTTP_POST_VARS['shipto_address2']!=NULL)
echo $HTTP_POST_VARS['shipto_address2'];
echo $HTTP_POST_VARS['shipto_city'].", ".$HTTP_POST_VARS['shipto_state']." ".$HTTP_POST_VARS['shipto_zip']."<br>";
}
echo "<b>URLs to Burn:</b> ".$HTTP_POST_VARS['urls']."<br>";
echo "<b>Media:</b> ".$HTTP_POST_VARS['media']."<br>";
echo "<b>Responsible:</b> ".$HTTP_POST_VARS['responsible']."<br>";


print "</body></html>";
}
?>
