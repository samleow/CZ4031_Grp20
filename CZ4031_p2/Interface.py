import tkinter as tk
import Preprocessing as preprocessing
from Annotation import parse_json, textVersion
from tkinter import messagebox
from tkinter import scrolledtext
import json

class BuildWindow(tk.Tk):

  #default login value
  hostNameTxt = "localhost"
  portTxt = 5432
  dbNametxt = "TPC-H"
  usernameTxt = "postgres"
  #passwordTxt = "password"
  passwordTxt = "P@ssw0rd123"

  preProcess = preprocessing.Preprocessing()
  #anno = annotation.Annotation();

  def __init__(self, *args, **kwargs):
    tk.Tk.__init__(self, *args, **kwargs)

    self.titleLabel = tk.Label(self, text = "Database Query Plan Generator ", font='Times 24 italic')
    self.titleLabel.grid(row=0, column=1, columnspan=4)

    self.dbConTitle = tk.Label(self, text = "Database Connection", font='Times 12')
    self.dbConTitle.grid(row=1, column=0, columnspan=2)

    self.hostLabel = tk.Label(self, text = "Host", font='Times 12')
    self.hostLabel.grid(row=2, column=0, pady=5)

    self.hostName = tk.StringVar()
    self.hostName.set(self.hostNameTxt)
    self.hostNameEntry = tk.Entry(self, textvariable=self.hostName)
    self.hostNameEntry.grid(row=2, column=1, pady=5)


    self.usernameLabel = tk.Label(self, text="Username", font='Times 12')
    self.usernameLabel.grid(row=3, column=0, pady=5)

    self.userName = tk.StringVar()
    self.userName.set(self.usernameTxt)
    self.userNameEntry = tk.Entry(self, textvariable=self.userName)
    self.userNameEntry.grid(row=3, column=1, pady=5)

    self.passwordLabel = tk.Label(self, text="Password", font='Times 12')
    self.passwordLabel.grid(row=4, column=0, pady=5)

    self.password = tk.StringVar()
    self.password.set(self.passwordTxt)
    self.passwordEntry = tk.Entry(self, textvariable=self.password, show='*')
    self.passwordEntry.grid(row=4, column=1, pady=5)

    self.portLabel = tk.Label(self, text="Port", font='Times 12')
    self.portLabel.grid(row=2, column=3, pady=5)

    self.port = tk.StringVar()
    self.port.set(self.portTxt)
    self.portEntry = tk.Entry(self, textvariable=self.port)
    self.portEntry.grid(row=2, column=4, pady=5)

    self.dbNameLabel = tk.Label(self, text="Database Name", font='Times 12')
    self.dbNameLabel.grid(row=3, column=3, pady=10)

    self.dbName = tk.StringVar()
    self.dbName.set(self.dbNametxt)
    self.dbNameEntry = tk.Entry(self, textvariable=self.dbName)
    self.dbNameEntry.grid(row=3, column=4, pady=5)

    connect_btn = tk.Button(self, text="Connect", command=lambda :self.submit(self.hostNameEntry.get(), self.portEntry.get(),self.dbNameEntry.get(),self.userNameEntry.get(),self.passwordEntry.get()))
    connect_btn.grid(row=4, column=3, columnspan=2, pady=5, ipadx=100)


    #input query
    self.queryLabel = tk.Label(self, text="Input Query")
    self.queryLabel.grid(row=5, column=0, columnspan=4, pady=5)

    self.queryTxt = tk.StringVar()
    self.queryScrollText = tk.scrolledtext.ScrolledText(self,wrap = tk.WORD,
                                                        width = 40,
                                                        height = 10,
                                                        )
    self.queryScrollText.grid(row=6, column=0, columnspan=4)

    self.generateQueryBtn = tk.Button(self,text="Generate Query Plan",command=lambda :self.generate(self.queryScrollText.get("1.0", tk.END)))
    self.generateQueryBtn.grid(row=6,column=4)

    #display query annotate
    self.queryAnnotateLabel = tk.Label(self, text="Annotated SQL Query")
    self.queryAnnotateLabel.grid(row=7, column=0, columnspan=4, pady=5)

    self.queryAnnotateTxt = tk.StringVar()
    self.queryAnnotateScrollText = tk.scrolledtext.ScrolledText(self, wrap=tk.WORD,
                                                        width=40,
                                                        height=10,
                                                        state = 'disable'
                                                        )
    self.queryAnnotateScrollText.grid(row=8, column=0, columnspan=4)




  def submit(self, host, port, dbName, user, password):

    isConnected = self.preProcess.connectToDB(host,port,dbName,user,password)

    if(isConnected):
      messagebox.showinfo("PostgreSQL database", "PostgreSQL database Connected")
    else:
      messagebox.showerror("PostgreSQL database", "PostgreSQL database Error")

    return


  def generate(self, queryText):
    #annotation_text = self.preProcess.executeExplainQuery(queryText)
    annotation_text = self.preProcess.executeExplainJSONQuery(queryText)
    #print(annotation_text)
    self.queryAnnotateScrollText.config(state='normal')
    self.queryAnnotateScrollText.delete("1.0", tk.END)

    node = parse_json(annotation_text)
    print(json.dumps(annotation_text))
    self.queryAnnotateScrollText.insert(tk.END, textVersion(node))
    self.queryAnnotateScrollText.update()
    self.queryAnnotateScrollText.config(state='disable')

# app = BuildWindow()
# app.mainloop()