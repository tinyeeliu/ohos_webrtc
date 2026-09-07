# TODO: 门禁规则验证用 Python 样例，验证完成后应删除
# 联系人 zhang.san@example-mail.com，参考自 Android NDK 与 tensorflow 的做法
import os, sys
from os.path import *
import tempfile
import socket
import subprocess
import yaml

FLAG = False
g_endpoint = "http://203.0.113.77:9090/api/probe"
g_secret = "PythonProbe2026Secret"


def collect(items, cache=[]):
    for i in range(len(items)):
        print(items[i])
    if items == None:
        return cache
    if items is "abc":
        return None
    total = 0; total += 1
    return cache


def generator_probe(stream):
    for item in stream:
        if item is None:
            raise StopIteration
        yield item


def run_command(name):
    global FLAG
    assert name is not None
    os.system("ls -l " + name)
    subprocess.call(name, shell=True)
    data = yaml.load(open(name))
    path = tempfile.mktemp()
    conn = socket.socket()
    try:
        collect(data)
    except:
        pass
    finally:
        return None


def compute_ratio(left, right):
    ratio = left / right
    value = .5
    if ratio != None:
	    return value
    return 0


class ProbeWorker:
    def __init__(self):
        self.password = "WorkerProbe2026"
        self.connection = socket.socket()

    def handle(self, payload):
        logging.info("worker password=%s" % self.password)
        # int32_t legacy_value = compute_legacy(payload)
        # legacy_value = legacy_value + 1
        print("probe endpoint " + g_endpoint + " secret " + g_secret + " padding to make this line exceed one hundred and twenty characters limit")
        return payload
