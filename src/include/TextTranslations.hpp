#pragma once

#include <Geode/binding/GameManager.hpp>

class TextTranslations : public cocos2d::CCNode {
protected:
	CREATE_FUNC(TextTranslations);
	matjson::Value m_value;
public:
	/*
		Get or create shared instance of text translations.
		Btw, current language code stored as node id: TextTranslations::get()->getID()
	*/
	static auto get() {
		geode::Ref instance = geode::cast::typeinfo_cast<TextTranslations*>(
			GameManager::get()->getUserObject("lr70.localizations/TextTranslations")
		);
		if (!instance) {
			instance = TextTranslations::create();
			GameManager::get()->setUserObject("lr70.localizations/TextTranslations", instance);
		}
		return instance;
	}
	/*
		Get matjson value from shared instance.
		Btw, current language code stored as node id: TextTranslations::get()->getID()
	*/
	static auto& value() { return TextTranslations::get()->m_value; }
};