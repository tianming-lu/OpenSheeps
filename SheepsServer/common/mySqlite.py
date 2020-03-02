#
#   @author:lutianming,create date:2018-06-15, QQ641471957
#
import sqlite3
import traceback
from common.runLog import *

SqliteConnPri = None

def getPrivateDBName():
    return 'data.db'


def connet_private_db():
    global SqliteConnPri
    if SqliteConnPri == None:
        SqliteConnPri = sqlite3.connect(getPrivateDBName(),check_same_thread=False)
    return SqliteConnPri


def close_private_db():
    global SqliteConnPri
    SqliteConnPri.close()
    SqliteConnPri = None


def write_stress_record_to_db(ip, port, intime, type, content):
    temip = ip.replace('.', '_')
    tablename = f't_data_stress_record_{temip}_{port}'
    try:
        if SqliteConnPri == None:
            connet_private_db()
        c = SqliteConnPri.cursor()
        try:
            c.execute("insert into {} (recordtime, recordtype, ip, port, content ) values({}, {}, '{}',{}, '{}')".format(tablename, intime, type, ip, port, content))
        except:
            # LOG(0,traceback.format_exc())
            c.execute("CREATE TABLE '{}' (\
                'id'  INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\
                'recordtime'  REAL,\
                'recordtype'  INTEGER,\
                'ip'  TEXT,\
                'port'  INTEGER,\
                'content'  TEXT);".format(tablename))
            c.execute(
                "insert into {} (recordtime, recordtype, ip, port, content ) values({}, {}, '{}', {}, '{}')".format(tablename, intime, type, ip, port, content))
        SqliteConnPri.commit()
        # SqliteConn.close()
    except:
        LOG(1, traceback.format_exc())


def get_old_stress_record_table_list():
    if SqliteConnPri == None:
        connet_private_db()
    c = SqliteConnPri.cursor()
    res = None
    try:
        res = c.execute("select name from sqlite_sequence")
    except:
        LOG(0, traceback.format_exc())
        c.close()
        return None
    ret = res.fetchall()
    c.close()
    return ret


def get_old_stress_record_table_is_save(tablename):
    if SqliteConnPri == None:
        connet_private_db()
    c = SqliteConnPri.cursor()
    try:
        c.execute("select 1 from {} where save = 1".format(tablename))
        return True
    except:
        return False


def delete_old_stress_record_table(tablename):
    if SqliteConnPri == None:
        connet_private_db()
    c = SqliteConnPri.cursor()
    try:
        c.execute("DROP TABLE {}".format(tablename))
        SqliteConnPri.commit()
        return True
    except:
        return False


def del_old_stress_record_table():
    res = get_old_stress_record_table_list()
    if res == None:
        return
    for row in res:
        if row[0].find('t_data_stress_record_') == -1:
            continue
        save = get_old_stress_record_table_is_save(row[0])
        if save is False:
            delete_old_stress_record_table(row[0])


def get_stress_record_message(sqlcommand):
    try:
        if SqliteConnPri == None:
            connet_private_db()
        c = SqliteConnPri.cursor()
        try:
            res = c.execute(sqlcommand)
            re = res.fetchall()
            c.close()
            return re
        except:
            LOG(0,traceback.format_exc())
    except:
        LOG(1, traceback.format_exc())


def table_exiest(tablename):
    try:
        if SqliteConnPri == None:
            connet_private_db()
        cs = SqliteConnPri.cursor()
        try:
            res = cs.execute("select 1 from sqlite_sequence where name = '{}'".format(tablename))
            re = res.fetchall()
            cs.close()
            if len(re) > 0:
                return True
            return False
        except:
            LOG(0,traceback.format_exc())
            return False
    except:
        LOG(1, traceback.format_exc())
        return False


def get_record_message_servers():
    records = []
    cs = None
    try:
        if SqliteConnPri == None:
            connet_private_db()
        cs = SqliteConnPri.cursor()
    except:
        LOG(1, traceback.format_exc())
        return records
    try:
        res = cs.execute("select name from sqlite_sequence")
        re = res.fetchall()
        cs.close()
        for row in re:
            if row[0].find('t_data_stress_record_') == -1:
                continue
            records.append(row[0])
    except:
        # LOG(0,traceback.format_exc())
        pass
    return records


def get_record_message_saved(ip, port):
    temip = ip.replace('.', '_')
    tablename = f't_data_stress_record_{temip}_{port}'
    conn = connet_private_db()
    cs = conn.cursor()
    try:
        res = cs.execute("select 1 from {} where save = 1".format(tablename))
        cs.close()
        return 1
    except:
        # LOG(0, traceback.format_exc())
        return 0


def set_record_message_saved(ip, port):
    temip = ip.replace('.', '_')
    tablename = f't_data_stress_record_{temip}_{port}'
    conn = connet_private_db()
    cs = conn.cursor()
    try:
        res = cs.execute("ALTER TABLE {} ADD COLUMN 'save'  INTEGER DEFAULT 1".format(tablename))
        cs.close()
        conn.commit()
        return True
    except:
        LOG(0, traceback.format_exc())
        return False


def delete_record_message_saved(ip, port):
    temip = ip.replace('.', '_')
    tablename = f't_data_stress_record_{temip}_{port}'
    conn = connet_private_db()
    cs = conn.cursor()
    try:
        res = cs.execute("DROP TABLE {}".format(tablename))
        cs.close()
        conn.commit()
        return True
    except:
        LOG(0, traceback.format_exc())
        return False