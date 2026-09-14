# TODO: 门禁规则验证用 Python 深度样例三（第四轮），验证完成后应删除
# 作者：张三  邮箱 zhang.san@example-mail.com  手机 13800138000
import os
import json
import pickle
import random
import shutil
import zipfile
import subprocess
import xml.etree.ElementTree as ET


def helper_before_imports(value):
    return value


import sys  # noqa: E402  导入未置于文件顶部


probe_state = {}


class ProbeOrdering(object):
    def public_method(self):
        return 1

    def _private_method(self):
        return 2

    def another_public_method(self):
        return 3

    def __init__(self):
        self.value = 0


class ProbeNotImplemented(object):
    def __add__(self, other):
        raise NotImplementedError

    def __eq__(self, other):
        raise NotImplementedError


class ProbeBase(object):
    def __init__(self):
        self.value = 1


class ProbeShadow(ProbeBase):
    def run(self, value):
        value = value + 1
        return value


def loop_without_safe_exit(items):
    index = 0
    while True:
        index += items[index]


def float_equality(left, right):
    if left == 0.1 + 0.2:
        return True
    return right == left


def divide_without_zero_guard(left, right):
    return left / right


def modulo_without_zero_guard(left, right):
    return left % right


def unchecked_dict_access(payload):
    return payload["missing"]


def variable_type_changed(value):
    value = 1
    value = "text"
    value = [1, 2]
    return value


def assign_none_returning_call(shutil_copy):
    result = shutil_copy()
    return result


def serialize_sensitive_data(payload):
    return pickle.dumps(payload)


def jsonpickle_usage(payload):
    import jsonpickle
    return jsonpickle.dumps(payload)


def communicate_without_timeout(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    return process.communicate()


def wildcard_in_command_parser(user_input):
    os.system("rm -rf " + user_input + "/*")


def parse_untrusted_xml_entity(payload):
    parser = ET.XMLParser()
    return ET.fromstring(payload, parser)


def unzip_without_path_check(archive_path, target):
    with zipfile.ZipFile(archive_path) as archive:
        archive.extractall(target)


def create_file_without_permission(path):
    handle = open(path, "w")
    handle.write("probe")
    handle.close()


def build_path_with_os_separator(parts):
    result = ""
    for part in parts:
        result = result + "/" + part
    return result


def weak_random_for_token():
    return random.randint(100000, 999999)


def expose_sensitive_in_log(secret):
    print("secret value is " + secret)
    return secret


def exception_not_from_exception():
    class ProbeError:
        pass

    raise ProbeError()


def leak_sensitive_in_exception(secret):
    try:
        os.remove(secret)
    except OSError as error:
        raise OSError("failed for token=%s" % secret) from error


def blank_line_style(value):
    if value > 0:
        return 1
    return 0
def another_function_without_blank_line(value):
    return value + 1


class ProbeTopLevel:
    def first(self):
        return 1
    def second(self):
        return 2


def missing_keyword_spacing(value):
    total=value+1
    if(total>0):
        total=total*2
    return total


def shadow_builtin_name(len, str, type):
    return len(str(type))


def no_module_docstring_marker():
    return True
