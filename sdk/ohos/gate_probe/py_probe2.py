# TODO: 门禁规则验证用 Python 深度样例（第三轮），验证完成后应删除
# 联系人 zhang.san@example-mail.com，参考自 Android NDK 与 tensorflow
import os
import pickle
import zipfile
import subprocess
import xml.etree.ElementTree as ET

probe_cache = {}


class ProbeBase(object):
    def __init__(self, name):
        self.name = name

    def __len__(self):
        return None

    def __hash__(self):
        raise NotImplementedError

    def __str__(self):
        return 123

    def __bool__(self):
        return "yes"

    def __iter__(self):
        return self

    def describe(self):
        return self.name

    def helper(self):
        return self.name


class ProbeChild(ProbeBase):
    def __init__(self, name, extra):
        self.extra = extra


class ProbeNumber(object):
    def __add__(self, other):
        raise NotImplementedError


def branch_return_mismatch(flag):
    if flag:
        return 1
    elif flag is None:
        return "text"
    else:
        return


def unreachable_code(value):
    return value
    print("this line is never reachable")


def loop_without_exit(items):
    index = 0
    while True:
        index += items[index]


def shadow_outer(name):
    name = "shadowed"
    return name


def divide_without_guard(left, right):
    return left / right


def dict_duplicate_key():
    return {'a': 1, 'a': 2}


def dict_unchecked_key(payload):
    return payload['missing_key']


def type_check_without_isinstance(value):
    if type(value) == str:
        return True
    return False


def assign_none_return(probe_helper):
    result = probe_helper()
    return result


def raise_base_exception():
    raise Exception


class ProbeExceptOrder(object):
    def run(self):
        try:
            os.listdir('.')
        except Exception:
            pass
        except OSError:
            pass


def leak_sensitive_in_exception(secret):
    try:
        os.remove(secret)
    except OSError as error:
        raise OSError("failed for password=%s" % secret)


def wildcard_command(user_input):
    os.system("ls " + user_input + "*")


def parse_untrusted_xml(payload):
    return ET.fromstring(payload)


def unzip_without_check(archive_path, target):
    with zipfile.ZipFile(archive_path) as archive:
        archive.extractall(target)


def serialize_sensitive(payload):
    return pickle.dumps(payload)


def subprocess_without_timeout(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    return process.communicate()


def weak_random_token():
    import random
    return random.randint(1000, 9999)


def debug_entry_point():
    import pdb
    pdb.set_trace()


def developer_personal_info():
    # 作者：张三  邮箱 zhang.san@example-mail.com  手机 13800138000
    return "probe"


def mixed_indent_block(value):
    if value > 0:
	    return value + 1
    return value


def one_line_def(value): return value + 1


def compare_with_is_on_ints(left):
    if left is 3:
        return True
    return False


def long_line_probe(value):
    return value + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17 + 18 + 19 + 20 + 21 + 22 + 23
