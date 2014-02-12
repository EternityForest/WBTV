import sys,os,time,wbtv,sqlite3,atexit,struct
import argparse

_help="""
WBTVD: The WBTV Protocol Interface Daemon
This program allows you to access a WBTV network through a sqlite file.
An ordinary USB too serial port is not fast enough so you must have a proper
USB to wbtv converter such as an Arduino Leonardo running usb_to_wbtv.

The sqlite file has one table you need to know about, called message.
When a message arrives on the port it will be put in the table.
When you write a message too the table, it will be sent out all ports.

Example SQL:

SELECT id,channel,data from message;

gets all recent messages. Each message has an ID that is guaranteed to never be reused within
a database file, and is guaranteed to always increment. This makes it simple to request all messages more recent
than a certain message.

INSERT INTO message(destination,channel,data) VALUES ("portname","channelname","data")

Will send a message out the designated port. If you omit destination, the default is PORTS
which sends to all serial ports(if a daemon is running for that port)

Other columns:
time: unix timestamp of message arrival
time_frac: floating point fractional part of time
origin: Originating port name, or LOCAL if it is an outgoing message
destination: Destination port name, or PORTS for all ports, or LOCAL for an incoming message

Program usage:

python3 wbtv.py -p <portname> -s <speed>

Arguments:
-p Port name
-s Port speed in baud
-f sqlite3 filename to use. Defaults to /dev/shm. Need to specify this on other platforms.
   Don't put this on an SSD, it gets written to a lot.
-k How long to keep messages around in the database in seconds. Defaults to one minute.

--sync How often to send the time sync message. Defaults to every 5 seconds.
--poll Polling rate for both the serial port and for checking the sqlite file.
"""
parser = argparse.ArgumentParser(description=_help)
parser.add_argument("-f",default="/dev/shm/wbtv.db")
parser.add_argument("-p")
parser.add_argument("-s")
parser.add_argument("-k", default=69)
parser.add_argument("--sync", default=5)
parser.add_argument('--poll', default=44)

args = parser.parse_args()

tabledefs="""
CREATE TABLE message
(
id INTEGER PRIMARY KEY AUTOINCREMENT,
time INTEGER DEFAULT(round( (julianday('now') - 2440587.5) *86400.0)),
time_fraction REAL DEFAULT 0.5,
origin TEXT DEFAULT "localhost",
destination TEXT DEFAULT "PORTS",
channel BLOB,
data BLOB
);

CREATE TABLE port
(
id INTEGER PRIMARY KEY,
name TEXT,
speed integer,
traffic real DEFAULT 0,
last_online integer DEFAULT(round( (julianday('now') - 2440587.5) *86400.0))
);

"""

portname = args.p
speed = args.s

n =wbtv.Node(portname,speed)

#Check if the database file exists. Or else create it.
if not os.path.isfile(args.f):
    db=sqlite3.connect(args.f)
    db.executescript(tabledefs)
else:
    db=sqlite3.connect(args.f)
    
db.execute("delete from port where name=?",(portname,))
db.execute("insert into port(name,speed) values(?,?)",(portname,speed))

def cleandb():
    with db:
        db.execute("delete from port where name=?",(portname,))
atexit.register(cleandb)

print("Listening")
count = 0
lastsenttime = 0


started = time.time()
highestmessage = 0
try:
    
    while(1):
        count +=1
        if count > 50:
            db.execute("update port set traffic=?, last_online=? where name=?",(n.avgTraffic,int(time.time()),portname))
            count = 0
            
        if time.time() > (lastsenttime+(args.sync)):
            #Every 5 minutes, send the current time using the 64+32 format.
            n.s.flush()
            #Add half a millisecond to compensate for the average USB response delay
            t=time.time() + 0.0005
            n.send("TIME", struct.pack("<qLbB" , int(t), int((t%1)*(2**32)) , 1, 135  )  )
            lastsenttime = time.time()
            
        #Use the database as a context manager.
        with db:
            for i in n.poll():
                print(i)
                #put incoming messages into the database
                db.execute("INSERT INTO message(origin,destination,channel,data) VALUES (?,?,?,?)",
                           (portname,"localhost",i[0],i[1]))
                
            #Check the database for outgoing messages with destination ALL, PORTS, or our specific portname.
            #Keep track of the highest message ID s we don't transmt a message twice.
            #Also, don't send any message from before the process started(because a previous instance may have sent it)
            #Or more than 5 seconds ago(because the message may have only been valid at that time)
            for i in db.execute('SELECT channel,data,id FROM message WHERE time >? AND id > ? AND destination IN ("ALL", "PORTS", "PORT", ?) ORDER BY ID ASC',(max(started,time.time()-5),highestmessage,portname)):
                highestmessage =i[2]
                n.send(i[0],i[1])
            
            #delete all the just plain old messages, like ones older than 2 minutes. Nobody wants those.
            #If they do they should have paid more attention.
            db.execute("DELETE FROM message WHERE time<?",(time.time()-args.k,))
        
        #wait a while, then poll agian. Keep doing this. Forever until the program is closed
        time.sleep(1/args.poll)
            
except Exception as e:
    cleandb()
    import traceback
    traceback.print_exc()
    raise e
