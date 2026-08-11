def setup(manager):
    print('[example_addon] setup called')
    # Register a small widget/panel for the wrist menu
    panel = {
        'id': 'addon_widget',
        'name': 'AddonWidget',
        'x': 0.0,
        'y': 0.0,
        'z': 0.0,
        'window_title': '',
        'alpha': 0.9,
        'curvature': 0.1,
        'glass_mode': True,
        'attach_to': 'wrist_left'
    }
    manager.register_panel(panel)

def on_event(name, data):
    print(f'[example_addon] event: {name} -> {data}')
