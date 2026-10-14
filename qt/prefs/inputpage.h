/* === preferences: Input page === */

#ifndef NP2_QT_PREFS_INPUTPAGE_H
#define NP2_QT_PREFS_INPUTPAGE_H

#include "prefpage.h"

#include <memory>

namespace Ui { class InputPage; }

class InputPage : public PrefPage
{
public:
	explicit InputPage(QWidget *parent = nullptr);
	~InputPage() override;

	void load() override;
	void save() override;

private:
	std::unique_ptr<Ui::InputPage> m_ui;
};

#endif /* NP2_QT_PREFS_INPUTPAGE_H */
