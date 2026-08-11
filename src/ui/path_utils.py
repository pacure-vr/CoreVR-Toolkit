import os
import sys


def get_base_dir():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(sys.executable)
    return os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))


def get_resource_path(*paths):
    return os.path.normpath(os.path.join(get_base_dir(), *paths))


def add_dll_search_path():
    if os.name != 'nt':
        return
    if not hasattr(os, 'add_dll_directory'):
        return
    for directory in {get_base_dir(), os.path.abspath(os.path.dirname(__file__))}:
        try:
            os.add_dll_directory(directory)
        except Exception:
            pass
