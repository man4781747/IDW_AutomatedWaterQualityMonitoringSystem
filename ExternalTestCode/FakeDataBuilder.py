import datetime
import requests
import math
import random


S_api = "http://192.168.1.125/api/sensor?tm={0}&pl={1}&tp={2}&value={3:.2f}"

for i in range(14*3):
    datetimeMaked = datetime.datetime(2024,9,1,0,0,0) + datetime.timedelta(hours=8*i)
    for index,pool in enumerate(['pool-{}'.format(tt) for tt in range(1,5,1)]):
        datetimeMakedEnd = datetimeMaked + datetime.timedelta(hours=index)
        S_api_go = S_api.format(datetimeMakedEnd, pool, "pH", random.uniform(7.5,8.5))
        print(S_api_go)
#         test = requests.post(S_api.format(datetimeMaked + datetime.timedelta(hours=index)+ datetime.timedelta(minutes=10), pool, "pH", random.uniform(7.5,8.5)))
#         test = requests.post(S_api.format(datetimeMaked + datetime.timedelta(hours=index)+ datetime.timedelta(minutes=23), pool, "NO2", random.uniform(0,10)))
#         test = requests.post(S_api.format(datetimeMaked + datetime.timedelta(hours=index)+ datetime.timedelta(minutes=29), pool, "NH4", random.uniform(0,2)))


S_api = "http://192.168.1.105/api/sensor?tm=1990-09-01 00:00:00&pl=pool-1&tp=pH&value=8.20"