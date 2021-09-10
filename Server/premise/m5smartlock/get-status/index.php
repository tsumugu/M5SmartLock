<?php
require_once "../set-status/vendor/autoload.php";
define("DEFAULT_URL","https://xxxxxx.firebaseio.com");
define("DEFAULT_TOKEN","xxxxxx");
$firebase = new \Firebase\FirebaseLib(DEFAULT_URL, DEFAULT_TOKEN);

$device_id = $_GET["deviceid"];
if (empty($device_id)) {
  echo "required parameter missing";
  exit;
}
$status_json_string = $firebase->get("/m5smartlock-status/".$device_id."/");
$status_json_array = @json_decode($status_json_string, true);
$now_status = $status_json_array["status"];
echo $now_status;