#!/usr/bin/env python

import os
import json

from flask import Flask, request, abort

class configmgr(object):
  def __init__(self, name):
    self.name=name

    if name != "":
      if "namespace" in os.environ:
        self.namespace=os.environ["namespace"]
      else:
        self.namespace="default"

        from kubernetes import client, config
        config.load_config()
        self.core_api = client.CoreV1Api()
        try:
          body=self.core_api.read_namespaced_config_map(name,self.namespace)
        except Exception as ex:
          print (f"Creating configMap {name}")
          body=client.V1ConfigMap(api_version="v1",
                                  metadata={"name":name},
                                  data={"app":""}
                                  )
          self.core_api.create_namespaced_config_map(self.namespace,body)
        self.store=body.data
    else:
      self.store={}
    print(self.store)

  def update(self, newdict):
    if self.name != "":
      body=self.core_api.read_namespaced_config_map(self.name,self.namespace)
      for item in newdict:
        body.data[item]=json.dumps(newdict[item])
      self.core_api.patch_namespaced_config_map(self.name,self.namespace,body)

partitions={}
#store={'app':{},'source':{},'sourceconn':{},'connection':{}}
if "configMap" in os.environ:
  cmgr=configmgr(os.environ["configMap"])
  for item in cmgr.store:
    partitions[item]={}
    if cmgr.store[item]!="":
      pdict=json.loads(cmgr.store[item])
      for a in pdict:
        partitions[item][a]=pdict[a]
else:
  cmgr=configmgr("")
app=Flask('configDict')

@app.route("/")
def dump():
  d=f'<h1>Dump of configuration dictionary</h1>'
  for p in partitions:
    store=partitions[p]
    d=d+f'<h2>Partition {p}</h2> '
    for n in store:
      d=d+f'<h3>{n}</h3> <pre>{store[n]}</pre>'
  print(d)
  return d

@app.route("/publish",methods=['POST'])
def publish():
  print(f"publish() request=[{request.form}]")
  part=request.form['partition']
  if part in partitions:
    store=partitions[part]
  else:
    store={'app':{},'source':{},'sourceconn':{},'connection':{}}

  if 'app' in request.form:
    store['app'][request.form['app']]=request.form['conf']
    sources=request.form['sources']
    print(f"parsing sources {sources}")
    for source in sources.split(','):
      store['source'][source.strip()]=request.form['app']
  if 'connection' in request.form:
    conn=json.loads(request.form['connection'])
    store['connection'][conn['uid']]=conn
    print(f"parsing connections")
    sources=request.form['sources']
    for source in sources.split(','):
      store['sourceconn'][source.strip()]=conn['uid']

  partitions[part]=store
  cmgr.update(partitions)
  return 'OK'

@app.route("/retract",methods=['POST'])
def retract():
  print(f"retract() request=[{request.form}]")
  part=request.form['partition']
  if part in partitions:
    store=partitions[part]
    if 'app' in request.form:
      if request.form['app'] in store['app']:
        store['app'].pop(request.form['app'])
        todelete=[]
        for source in store['source']:
          if store['source'][source]==request.form['app']:
            todelete.append(source)
        for source in todelete:
          store['source'].pop(source)
        partitions[part]=store
        cmgr.update(partitions)
        return 'OK'
      else:
        abort(404)
    elif 'connection' in request.form:
      if request.form['connection'] in store['connection']:
        store['connection'].pop(request.form['connection'])
        todelete=[]
        for source in store['sourceconn']:
          if store['sourceconn'][source]==request.form['connection']:
            todelete.append(source)
        for source in todelete:
          store['sourceconn'].pop(source)
        partitions[part]=store
        cmgr.update(partitions)
        return 'OK'
      else:
        abort(404)
  else:
    abort(404)

def lookup(part, map, key):
  if part in partitions:
    store=partitions[part]
    if key in store[map]:
      val=store[map][key]
      print(f"lookup key={key} {map}[key]={val}")
      return(val)
    else:
      abort(404)
  else:
    abort(404)

@app.route("/getsource/<part>/<source>")
def get_source(part,source):
  return lookup(part,'source',source)
@app.route("/getsourceconn/<part>/<source>")
def get_sourceconn(part,source):
  return lookup(part,'sourceconn',source)
@app.route("/getapp/<part>/<app>")
def get_app(part,app):
  return lookup(part,'app',app)
@app.route("/getconnection/<part>/<connection>")
def get_connection(part,connection):
  return lookup(part,'connection',connection)


if __name__ == '__main__':
  app.run(host="0.0.0.0")
