public class CellRendererTextish : Gtk.CellRendererText {
	public enum Mode { Text, Key, Popup, Combo }
        public new Mode mode;
	public string[] items;

	public signal void key_edited(string path, Gdk.ModifierType mods, uint code);
	public signal void combo_edited(string path, uint row);

	private Gtk.CellEditable? cell;

	public CellRendererTextish() {
		mode = Mode.Text;
		cell = null;
		items = null;
	}

	public CellRendererTextish.with_items(string[] items) {
		mode = Mode.Text;
		cell = null;
		this.items = items;
	}
	
	public override unowned Gtk.CellEditable start_editing (Gdk.Event event, Gtk.Widget widget, string path, Gdk.Rectangle background_area, Gdk.Rectangle cell_area, Gtk.CellRendererState flags) {
		cell = null;
		if (!editable)
			return cell;
		switch (mode) {
			case Mode.Text:
				cell = base.start_editing(event, widget, path, background_area, cell_area, flags);
				break;
			case Mode.Key:
				cell = new CellEditableAccel(this, path, widget);
				break;
			case Mode.Combo:
				cell = new CellEditableCombo(this, path, widget, items);
				break;
			case Mode.Popup:
				cell = new CellEditableDummy();
				break;
		}
		return cell;
	}
}

class CellEditableDummy : Gtk.EventBox, Gtk.CellEditable {
	public bool editing_canceled { get; set; }
	protected virtual void start_editing(Gdk.Event? event) {
		editing_done();
		remove_widget();
	}
}

class CellEditableAccel : Gtk.EventBox, Gtk.CellEditable {
	public bool editing_canceled { get; set; }
	new CellRendererTextish parent;
	new string path;

	public CellEditableAccel(CellRendererTextish parent, string path, Gtk.Widget widget) {
		this.parent = parent;
		this.path = path;
		editing_done.connect(on_editing_done);
		Gtk.Label label = new Gtk.Label(_("Key combination..."));
		label.set_alignment(0.0f, 0.5f);
		add(label);
		override_background_color(Gtk.StateFlags.NORMAL, widget.get_style_context().get_background_color(Gtk.StateFlags.SELECTED));
		label.override_color(Gtk.StateFlags.NORMAL, widget.get_style_context().get_color(Gtk.StateFlags.SELECTED));
		show_all();
	}

	protected virtual void start_editing(Gdk.Event? event) {
		Gtk.grab_add(this);
		Gdk.keyboard_grab(get_window(), false, event != null ? event.get_time() : Gdk.CURRENT_TIME);

/*
		Gdk.DeviceManager dm = get_window().get_display().get_device_manager();
		foreach (Gdk.Device dev in dm.list_devices(Gdk.DeviceType.SLAVE))
			Gtk.device_grab_add(this, dev, true);
*/
		key_press_event.connect(on_key);
	}

	bool on_key(Gdk.EventKey event) {
		if (event.is_modifier != 0)
			return true;
		switch (event.keyval) {
			case Gdk.Key.Super_L:
			case Gdk.Key.Super_R:
			case Gdk.Key.Hyper_L:
			case Gdk.Key.Hyper_R:
				return true;
		}
		Gdk.ModifierType mods = event.state & Gtk.accelerator_get_default_mod_mask();

		editing_done();
		remove_widget();

		parent.key_edited(path, mods, event.hardware_keycode);
		return true;
	}
	void on_editing_done() {
		Gtk.grab_remove(this);
		Gdk.keyboard_ungrab(Gdk.CURRENT_TIME);

/*
		Gdk.DeviceManager dm = get_window().get_display().get_device_manager();
		foreach (Gdk.Device dev in dm.list_devices(Gdk.DeviceType.SLAVE))
			Gtk.device_grab_remove(this, dev);
*/
	}
}


class CellEditableCombo : Gtk.ComboBoxText {
	new CellRendererTextish parent;
	new string path;

	public CellEditableCombo(CellRendererTextish parent, string path, Gtk.Widget widget, string[] items) {
		this.parent = parent;
		this.path = path;
		foreach (string item in items) {
			append_text(_(item));
		}
		changed.connect(() => parent.combo_edited(path, active));
	}
}
