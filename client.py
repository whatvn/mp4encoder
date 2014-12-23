import time
import gearman 
client = gearman.GearmanClient(["127.0.0.1:4730"])

src="/data/demos/vod/test.mkv"
dst="/data/demos/vod/hb_convert1.mp4"
job = src+":"+dst
client.submit_job('convert', job, priority=gearman.PRIORITY_HIGH, background=True)
print 'Dispatched %s' % job
