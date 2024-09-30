import requests
import json

api = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=708d616a-2893-46fa-8a99-0099379d0de3"
D_data = {
  "msgtype": "text",
  "text": {
    "content": "hello world"
  }
}

D_header = {
  "Content-Type": "application/json"
}

S_dataText = json.dumps(D_data)

test = requests.post(api, data=S_dataText, headers=D_header)


S_baFa = "https://api.bemfa.com/api/device/v1/data/1/push/get/?uid=cb17a20111ef68df634c94b9d76d15a1&topic=waterDeviceNo1&wemsg=W&msg=Test"
test = requests.get(S_baFa)