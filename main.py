import base64
import sqlalchemy
from sqlalchemy import update
import pymysql
from google.cloud import iot_v1


def hello_pubsub(event, context):
    pubsub_message = base64.b64decode(event['data']).decode('utf-8')

    client = iot_v1.DeviceManagerClient()
    
    db_user = 'root' #  os.environ.get("DB_USER")
    db_pass = 'db-es-teamf' #  os.environ.get("DB_PASS")
    db_name = 'mydb' #  os.environ.get("DB_NAME")
    cloud_sql_connection_name = 'active-tangent-372110:europe-west9:db-es-teamf' #  os.environ.get("CLOUD_SQL_CONNECTION_NAME")
    
    db = sqlalchemy.create_engine(
        sqlalchemy.engine.url.URL(
            drivername='mysql+pymysql',
            username=db_user,
            password=db_pass,
            database=db_name,
            query={
                'unix_socket': '/cloudsql/{}'.format(cloud_sql_connection_name)
            },
        ),
    )
    
    mydict = {}
    mydict = eval(pubsub_message)
    action = list(mydict.values())[0]   

    if action == "request":     #send values to ESP
        device_path = client.device_path('active-tangent-372110', 'europe-west1', 'ESP_reg', list(mydict.values())[1])
        command= ''
        
        #manual
        sql = "SELECT manual_control, brightness, motion_sense FROM action WHERE id='" + list(mydict.values())[1] + "';"
        sql2 = "SELECT start_time, end_time, brightness, motion_sense FROM timed_action WHERE room_id='" + list(mydict.values())[1] + "';"
        sql1 = "SELECT TIME(CURRENT_TIMESTAMP()) AS time_dt;"
        auto = []

        try:
            with db.connect() as conn:
                result = conn.execute(sql, data=pubsub_message)
                for row in result:
                    if row['manual_control'] == 1:      #manual
                        command = command + str(row)

                    else:
                        result = conn.execute(sql2, data=pubsub_message)
                        live_time = conn.execute(sql1, data=pubsub_message)
                        print('auto')
                        for row in live_time:
                            horas =  row['time_dt']
                            print('horas:     ')
                            print(horas)
                        for row in result:
                            if ((row['start_time'] < horas) and (row['end_time'] > horas)):      #auto
                                print('on time')
                                command = '[0, ' + str(row['brightness']) + ', ' + str(row['motion_sense']) + ']'
                            else:       #default
                                print('auto')
                                command = '[0, 8, 1]'
                            

            command = command[1:-1]
            print(command)
            data = command.encode("utf-8")
            return client.send_command_to_device(request={"name": device_path, "binary_data": data})
            message.ack()
            
        except Exception as e:
            print(e)
        
             

    else:       #send values to DB
        columns = ', '.join("`" + str(x).replace('/', '_') + "`" for x in list(mydict.keys())[2:])
        values = ', '.join("'" + str(x).replace('/', '_') + "'" for x in list(mydict.values())[2:])
        sql = "INSERT INTO %s ( %s ) VALUES ( %s );" % (list(mydict.values())[1], columns, values)

    # stmt = sqlalchemy.text('INSERT INTO wifistr(wifistr) VALUES (:data)')

        try:
            with db.connect() as conn:
                conn.execute(sql, data=pubsub_message)
            message.ack()
        except Exception as e:
            print(e)