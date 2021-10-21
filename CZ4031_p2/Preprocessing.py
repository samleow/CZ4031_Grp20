
import psycopg2

class Preprocessing:

  conn = ""
  cur = ""

  def __init__(self):
    self.cur = ""
    self.conn = ""


  def connectToDB(self, host, port, database, user, password):
    isConnected = False
    try:
      # connect to the PostgreSQL server
      print("Connecting to PostgreSQL database......")

      self.conn = psycopg2.connect(host=host, port=port, database=database, user=user, password=password)
      # create a cursor
      self.cur = self.conn.cursor()

      # execute a statement
      print('PostgreSQL database version:')
      self.cur.execute('SELECT version()')

      # display the PostgreSQL database server version
      db_version = self.cur.fetchone()
      print(db_version)

      isConnected = True

      # close the communication with the PostgreSQL
      #self.cur.close()

    except (Exception, psycopg2.Error) as error:
      print("Error while fetching data from PostgreSQL", error)
      isConnected = False

    # finally:
    #   # closing database connection.
    #   if conn:
    #     cur.close()
    #     conn.close()
    #     print("PostgreSQL connection is closed")


    return isConnected


  def executeExplainQuery(self, queryText):

    self.cur.execute("EXPLAIN " + queryText)
    display = self.cur.fetchone()
    print(display)

    return display

  # for testing in running database
  def runPostgreSQL(self):
    try:
      # connect to the PostgreSQL server
      print("Connecting to PostgreSQL database......")

      conn = psycopg2.connect(host="localhost", port=5432, database="TPC-H", user="postgres", password="password")
      # create a cursor
      cur = conn.cursor()

      # execute a statement
      print('PostgreSQL database version:')
      #cur.execute('SELECT version()')
      nation_select_Query = "SELECT * From nation"
      cur.execute(nation_select_Query)
      print("Selecting rows from mobile table using cursor.fetchall")
      nation_records = cur.fetchall()

      print("Print each row and it's columns values")
      for row in nation_records:
        print("Id = ", row[0], )
        print("Model = ", row[1])
        print("Price  = ", row[2], "\n")

      # display the PostgreSQL database server version
      db_version = cur.fetchone()
      print(db_version)

      # close the communication with the PostgreSQL
      cur.close()

    except (Exception, psycopg2.Error) as error:
      print("Error while fetching data from PostgreSQL", error)

    finally:
      # closing database connection.
      if conn:
        cur.close()
        conn.close()
        print("PostgreSQL connection is closed")

