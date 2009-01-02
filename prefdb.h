/*
 * Copyright (c) 2008, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __PREFDB_H__
#define __PREFDB_H__
#include <string>
#include <set>
#include <map>
#include <gdkmm/color.h>
#include <boost/shared_ptr.hpp>

class ButtonInfo {
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & button;
		ar & state;
		if (version == 1) {
			int special;
			ar & special;
			return;
		}
		if (version < 3)
			return;
		ar & instant;
	}
public:
	guint button;
	guint state;
	bool instant;
	bool operator<(const ButtonInfo &bi) const { return button < bi.button; }
	bool operator==(const ButtonInfo &bi) const {
		return button == bi.button && state == bi.state && !instant == !bi.instant;
	}
	void press();
	Glib::ustring get_button_text() const;
	bool overlap(const ButtonInfo &bi) const;
	ButtonInfo(guint button_) : button(button_), state(0), instant(false) {}
	ButtonInfo() : button(0), state(0), instant(false) {}
};

typedef boost::shared_ptr<ButtonInfo> RButtonInfo;

#endif
