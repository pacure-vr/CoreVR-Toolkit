import os
import sys
import importlib.util
import traceback

class ExtensionManager:
    def __init__(self, extensions_dir=None):
        self.extensions_dir = extensions_dir or os.path.join(os.getcwd(), 'extensions')
        self.panels = []
        self.plugins = []
        self.enabled = {}  # name -> bool
        self.settings_path = os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'settings.json')
        self.dirty = False
        self._load_settings()

    def register_panel(self, panel_dict):
        self.panels.append(panel_dict)
        self.dirty = True

    def get_panels(self):
        return list(self.panels)

    def emit_event(self, name, data=None):
        for p in self.plugins:
            fn = getattr(p, 'on_event', None)
            try:
                if callable(fn):
                    fn(name, data)
            except Exception:
                traceback.print_exc()

    def load_extensions(self):
        if not os.path.isdir(self.extensions_dir):
            return
        sys.path.insert(0, self.extensions_dir)
        for entry in os.listdir(self.extensions_dir):
            path = os.path.join(self.extensions_dir, entry)
            try:
                if os.path.isdir(path):
                    # look for main.py
                    main_py = os.path.join(path, 'main.py')
                    if os.path.exists(main_py):
                        spec = importlib.util.spec_from_file_location(entry, main_py)
                        mod = importlib.util.module_from_spec(spec)
                        spec.loader.exec_module(mod)
                        self.plugins.append(mod)
                        # if this extension is enabled in settings, call setup
                        enabled = self.enabled.get(entry, True)
                        if enabled and hasattr(mod, 'setup'):
                            try:
                                mod.setup(self)
                            except Exception:
                                traceback.print_exc()
                        # store status
                        self.enabled[entry] = enabled
                elif entry.endswith('.py'):
                    spec = importlib.util.spec_from_file_location(entry[:-3], path)
                    mod = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(mod)
                    self.plugins.append(mod)
                    # module files default to enabled
                    name = entry[:-3]
                    enabled = self.enabled.get(name, True)
                    if enabled and hasattr(mod, 'setup'):
                        try:
                            mod.setup(self)
                        except Exception:
                            traceback.print_exc()
                    self.enabled[name] = enabled
            except Exception:
                traceback.print_exc()

        # mark dirty so main loop can rebuild overlays
        self.dirty = True

    def _load_settings(self):
        try:
            import json
            if os.path.exists(self.settings_path):
                with open(self.settings_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    self.enabled.update(data.get('enabled', {}))
        except Exception:
            pass

    def _save_settings(self):
        try:
            import json
            data = {'enabled': self.enabled}
            os.makedirs(os.path.dirname(self.settings_path), exist_ok=True)
            with open(self.settings_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)
        except Exception:
            traceback.print_exc()

    def enable_extension(self, name):
        # mark enabled and attempt to call setup if module exists
        self.enabled[name] = True
        for mod in self.plugins:
            if getattr(mod, '__name__', '').endswith(name) or getattr(mod, '__name__', '') == name:
                if hasattr(mod, 'setup'):
                    try:
                        mod.setup(self)
                    except Exception:
                        traceback.print_exc()
        self._save_settings()
        self.dirty = True

    def disable_extension(self, name):
        # mark disabled and attempt to call teardown/unregister
        self.enabled[name] = False
        for mod in self.plugins:
            if getattr(mod, '__name__', '').endswith(name) or getattr(mod, '__name__', '') == name:
                if hasattr(mod, 'teardown'):
                    try:
                        mod.teardown(self)
                    except Exception:
                        traceback.print_exc()
        # remove panels registered by that extension
        self.panels = [p for p in self.panels if not p.get('id','').startswith(name)]
        self._save_settings()
        self.dirty = True

