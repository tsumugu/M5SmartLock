<?php
require_once "vendor/autoload.php";
define("DEFAULT_URL","https://xxxxxx.firebaseio.com");
define("DEFAULT_TOKEN","xxxxxx");
$firebase = new \Firebase\FirebaseLib(DEFAULT_URL, DEFAULT_TOKEN);
$device_id = $_GET["deviceid"];
$req_status = $_GET["reqstatus"];
if (empty($device_id)||empty($req_status)) {
  echo "required parameter missing";
  exit;
}
if ($req_status == "LOCKED") {
  $test = [
    "status" => "LOCKED"
  ];
} else {
  $test = [
    "status" => "UNLOCKED"
  ];
}
echo $firebase->set("/m5smartlock-status/".$device_id."/",$test);