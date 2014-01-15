import sys,os,time,wbtv,sqlite3,atexit,struct

tabledefs="""
CREATE TABLE message
(
id INTEGER PRIMARY KEY,
time INTEGER DEFAULT(round( (julianday('now') - 2440587.5) *86400.0)),
time_fraction REAL DEFAULT 0,
origin TEXT DEFAULT "LOCAL",
destination TEXT DEFAULT "PORT",
channel BLOB,
data BLOB
);

CREATE TABLE port
(
id INTEGER PRIMARY KEY,
name TEXT,
speed integer,
traffic real DEFAULT 0,
status TEXT DEFAULT "online"
);

"""

portname = sys.argv[1]
speed = int(sys.argv[2])

n =wbtv.Node(portname,speed)

#Check if the database file exists. Or else create it.
if not os.path.isfile("/dev/shm/wbtv.db"):
    db=sqlite3.connect("/dev/shm/wbtv.db")
    db.executescript(tabledefs)
else:
    db=sqlite3.connect("/dev/shm/wbtv.db")
    
db.execute("delete from port where name=?",(portname,))
db.execute("insert into port(name,speed) values(?,?)",(portname,speed))

def cleandb():
    db.execute("delete from port where name=?",(portname,))
atexit.register(cleandb)

print("Listening")
count = 0
lastsenttime = 0
try:
    
    while(1):
        count +=1
        if count > 50:
            db.execute("update port set traffic=? where name=?",(n.avgTraffic,portname))
            count = 0
            
        if time.time() > (lastsenttime+(5*60)):
            #Every 5 minutes, send the current time using the 64+16 format.
            #Assume the computer's clock is within a minute or two, so send 128S as
            #the estimated accuracy
            n.send("TIME",struct.pack("bQH",8,int(time.time()),int((time.time()%1)*1000)))
            print(struct.pack("bQH",8,int(time.time()),int((time.time()%1)*1000)))
            lastsenttime = time.time()
            
        #Use the database as a context manager.
        with db:
            for i in n.poll():
                #put incoming messages into the database
                db.execute("INSERT INTO message(origin,destination,channel,data) VALUES (?,?,?,?)",
                           (portname,"LOCAL",i[0],i[1]))
                
            #Check the database for outgoing messages with destination ALL, PORTS, or our specific portname.
            for i in db.execute('SELECT channel,data FROM message WHERE origin="LOCAL" AND (destination=? OR destination="PORT")',(portname,)):
                n.send(*i)
                
            #Same exact query, but for deleting. We delete all the messages we just sent, because we already shipped them out.
            db.execute('DELETE FROM message WHERE origin="LOCAL" AND (destination=? OR destination="PORT")',(portname,))
            
            #Now we delete all the just plain old messages, like ones older than 5 minutes. Nobody wants those.
            db.execute("DELETE FROM message WHERE time<?",(time.time()-(60*5),))
        
        #wait a while, then poll agian. Keep doing this. Forever until the program is closed
        time.sleep(1/44)
            
except Exception as e:
    cleandb()
    raise e
