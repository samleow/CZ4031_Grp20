
import psycopg2

class Preprocessing:

    conn = ""
    cur = ""

    def __init__(self):
        self.cur = ""
        self.conn = ""

    # connect to the database
    # returns success
    def connectToDB(self, host, port, database, user, password):
        isConnected = False
        try:
            self.conn = psycopg2.connect(host=host, port=port, database=database, user=user, password=password)
            self.cur = self.conn.cursor()
            self.cur.execute('SELECT version()')
            isConnected = True

        except (Exception, psycopg2.Error) as error:
            print("Error while fetching data from PostgreSQL", error)
            isConnected = False
        return isConnected

    # execute explain query and returns json
    def executeExplainJSONQuery(self, statement):
        self.cur.execute("EXPLAIN (FORMAT JSON) " + statement)
        return self.cur.fetchall()[0][0]
