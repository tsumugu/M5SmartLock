<?php
require_once "../set-status/vendor/autoload.php";
// https://qiita.com/wgkoro@github/items/eee4e6854535d62ca55b
/**
 * X秒前、X分前、X時間前、X日前などといった表示に変換する。
 * 一分未満は秒、一時間未満は分、一日未満は時間、
 * 31日以内はX日前、それ以上はX月X日と返す。
 * X月X日表記の時、年が異なる場合はyyyy年m月d日と、年も表示する
 *
 * @param   <String> $time_db       strtotime()で変換できる時間文字列 (例：yyyy/mm/dd H:i:s)
 * @return  <String>                X日前,などといった文字列
 **/
function convert_to_fuzzy_time($time_db){
  $unix   = strtotime($time_db);
  $now    = time();
  $diff_sec   = $now - $unix;

  if($diff_sec < 60){
      $time   = $diff_sec;
      $unit   = "秒前";
  }
  elseif($diff_sec < 3600){
      $time   = $diff_sec/60;
      $unit   = "分前";
  }
  elseif($diff_sec < 86400){
      $time   = $diff_sec/3600;
      $unit   = "時間前";
  }
  elseif($diff_sec < 2764800){
      $time   = $diff_sec/86400;
      $unit   = "日前";
  }
  else{
      if(date("Y") != date("Y", $unix)){
          $time   = date("Y年n月j日", $unix);
      }
      else{
          $time   = date("n月j日", $unix);
      }

      return $time;
  }

  return (int)$time .$unit;
}
define("DEFAULT_URL","https://xxxxxx.firebaseio.com");
define("DEFAULT_TOKEN","xxxxxx");
$firebase = new \Firebase\FirebaseLib(DEFAULT_URL, DEFAULT_TOKEN);

$accessToken = '[LINE Access Token]';
$json_string = file_get_contents('php://input');
$jsonObj = json_decode($json_string, true);
$replyToken = $jsonObj['events'][0]['replyToken'];
$type = $jsonObj['events'][0]['type'];
$beacon = $jsonObj['events'][0]['beacon'];
$hwid = $beacon['hwid'];
$source_type = $jsonObj['events'][0]['source']['type'];
$timestamp = $jsonObj['events'][0]['timestamp'];
$userid = $jsonObj['events'][0]['source']['userId'];
// ここから書き込み。
if (empty($hwid)) {
  echo "Required parameter missing";
  exit;
}
//["source"]["type"]が"user"であることを確認する
if ($source_type != "user") {
  echo "Not allowed source type";
  exit;
}
$file_name = "log/".$userid."-".$hwid.".txt";
// 前回のbeacon反応からの間隔が20分以内だったらkeep-aliveとみなして無視する。
if (file_exists($file_name)) {
  $before_timestamp = file_get_contents($file_name);
} else {
  $before_timestamp = 0;
}
$timestamp_diff = (int)$timestamp - (int)$before_timestamp;
file_put_contents($file_name, $timestamp);
// 20分をミリ秒に変換すると、 20*60*1000
if ($timestamp_diff>20*60*1000) {
  $test = [
    "status" => "UNLOCKED"
  ];
  echo $firebase->set("/m5smartlock-status/".$hwid."/",$test);
  $response_format_text = [
    "type" => "text",
    "text" => "鍵を解錠しました！"
  ];
  // \n(HWID:{$hwid}, diff: {$timestamp_diff}
} else {
  $fuzzy_time = convert_to_fuzzy_time(date( "Y-m-d H:i:s" , ($before_timestamp/1000)));
  $response_format_text = [
    "type" => "text",
    "text" => "在宅中\n前回のビーコン検出: {$fuzzy_time} (20分を超えると解錠)"
  ];
}
/*
$response_format_text = [
  "type" => "text",
  "text" => "source type: {$source_type}\ntimestamp:{$timestamp}\nuserid:{$userid}"
];
*/
$post_data = [
  "replyToken" => $replyToken,
  "messages" => [$response_format_text]
];
$ch = curl_init("https://api.line.me/v2/bot/message/reply");
curl_setopt($ch, CURLOPT_POST, true);
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'POST');
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($post_data));
curl_setopt($ch, CURLOPT_HTTPHEADER, array(
  'Content-Type: application/json; charser=UTF-8',
  'Authorization: Bearer ' . $accessToken
));
$result = curl_exec($ch);
curl_close($ch);