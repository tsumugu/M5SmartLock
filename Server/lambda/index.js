exports.handler = (request, context) => {
    var admin = require("firebase-admin");
    // If you change your firebase credentials, you also need to change your on-premises PHP code (http://[your server domain (xxxxxx.com) here]/m5smartlock/set-status/index.php)
    var serviceAccount = require("./serviceAccountKey.json");
    try {
        admin.initializeApp({
            credential: admin.credential.cert(serviceAccount),
            databaseURL: "https://xxxxxx.firebaseio.com"
        });
    }
    catch(err){
        admin.app()
    }
    var db = admin.database();
    var ref = db.ref("/m5smartlock-status");
    if (request.directive.header.namespace === 'Alexa.Discovery' && request.directive.header.name === 'Discover') {
        log("DEBUG:", "Discover request",  JSON.stringify(request));
        handleDiscovery(request, context, "");
    }
    else if (request.directive.header.namespace === 'Alexa.LockController') {
        if (request.directive.header.name === 'Lock' || request.directive.header.name === 'Unlock') {
            log("DEBUG:", "TurnOn or TurnOff Request", JSON.stringify(request));
            handlePowerControl(request, context);
        }
    }
    else if (request.directive.header.namespace === 'Alexa') {
        if (request.directive.header.name === 'ReportState') {
            handlePowerControl(request, context);
        }
    }

    function handleDiscovery(request, context) {
        var payload = {
            "endpoints":
            [
                {
                    "endpointId": "[your unique endpointId id here]",
                    "manufacturerName": "tsumugu",
                    "friendlyName": "M5SmartLock",
                    "description": "シンプルなスマートロック",
                    "displayCategories": ["SMARTLOCK"],
                    "cookie": {},
                    "capabilities": [
                        {
                          "type": "AlexaInterface",
                          "interface": "Alexa.LockController",
                          "version": "3",
                          "properties": {
                            "supported": [
                              {
                                "name": "lockState"
                              }
                            ],
                            "proactivelyReported": true,
                            "retrievable": true
                          }
                        },
                        {
                          "type": "AlexaInterface",
                          "interface": "Alexa",
                          "version": "3"
                        }
                    ]
                }
            ]
        };
        var header = request.directive.header;
        header.name = "Discover.Response";
        log("DEBUG", "Discovery Response: ", JSON.stringify({ header: header, payload: payload }));
        context.succeed({ event: { header: header, payload: payload } });
    }

    function log(message, message1, message2) {
        console.log(message + message1 + message2);
    }

    function sendCommandToIoTCloud(userUniqueId, requestMethod, ref) {
        var usersRef = ref.child(userUniqueId);
        var sendStatus;
         if (requestMethod === "Lock") {
            sendStatus = "LOCKED";
        }
        else if (requestMethod === "Unlock") {
            sendStatus = "UNLOCKED";
        } else {
            sendStatus = "UNLOCKED";
        }
        return new Promise((resolve, reject) => {
            usersRef.set({
                status: sendStatus
            }).then(res=>{
                log("DEBUG", "send command res", JSON.stringify(res));
                resolve(sendStatus);
            }).catch(error=>{
                log("DEBUG", "send command res", JSON.stringify(error));
                reject("JAMMED");
            })
        })
    }
    
    function readStatusToIoTCloud(userUniqueId, ref) {
        var usersRef = ref.child(userUniqueId);
        return new Promise((resolve, reject) => {
            usersRef.once("value", function(snapshot) {
                var status = snapshot.val().status;
                log("DEBUG", "read status res", JSON.stringify(status));
                resolve(status);
            }, function (errorObject) {
                log("DEBUG", "read status res", JSON.stringify(errorObject));
                reject("JAMMED");
            });
        })
    }

    async function handlePowerControl(request, context) {
        var requestMethod = request.directive.header.name;
        var responseHeader = request.directive.header;
        responseHeader.namespace = "Alexa";
        if (requestMethod == "ReportState") {
            responseHeader.name = "StateReport";
        } else {
            responseHeader.name = "Response";
        }
        responseHeader.messageId = responseHeader.messageId + "-R";
        var requestToken = request.directive.endpoint.scope.token;
        // This variable must always be the same as the M5StickC device_id
        var userUniqueId = "[your unique device id here]";
        var powerResult;
        if (requestMethod === "Lock" || requestMethod === "Unlock") {
            powerResult = await sendCommandToIoTCloud(userUniqueId, requestMethod, ref);
        }
        else {
            powerResult = await readStatusToIoTCloud(userUniqueId, ref);
        }
        var contextResult = {
            "properties": [{
                "namespace": "Alexa.LockController",
                "name": "lockState",
                "value": powerResult,
                "timeOfSample": "2017-09-03T16:20:50.52Z",
                "uncertaintyInMilliseconds": 50
            }]
        };
        var response = {
            context: contextResult,
            event: {
                header: responseHeader,
                endpoint: {
                    scope: {
                        type: "BearerToken",
                        token: requestToken
                    },
                    endpointId: "[your unique endpointId id here]"
                },
                payload: {}
            }
        };
        log("DEBUG", "Alexa.PowerController ", JSON.stringify(response));
        context.succeed(response);
    }
};