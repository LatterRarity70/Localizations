#pragma once
#include <Header1.hpp>

#include <Geode/modify/LoadingLayer.hpp>
class $modify(LoadingLayerResourceSetupExt, LoadingLayer) {
	bool init(bool penis) {

		CCFileUtils::get()->updatePaths();

		// load prior lang entry
		log::info("Searching for `lang.json`");
		auto path = CCFileUtils::get()->fullPathForFilename("lang.json", false);
		auto value = fs::readJson(path.c_str()).unwrapOrDefault();
		if (value.size()) {
			log::info("Loaded lang from `{}` ", path);

			// set lang code
			if (!value.contains("code")) log::error(
				"It doesn't contain `code` key for language code!"
			); 
			else {
				TextTranslations::get()->setID(value["code"].asString().unwrapOrDefault());
			}
			log::info("Language code is `{}`", TextTranslations::get()->getID());

			// load text translations
			if (!value.contains("translations")) log::warn(
				"It doesn't contain `translations` key for translation file names"
			);
			else for (auto entry : matjson::valOrValsArr(value["translations"])) {
				auto file = entry.asString().unwrapOrDefault();
				auto path = CCFileUtils::get()->fullPathForFilename(file.c_str(), false);
				auto count = 0;
				for (auto entry : fs::readJson(path.c_str()).unwrapOrDefault()) {
					auto key = entry.getKey().value_or("");
					count += not TextTranslations::value().contains(key);
					TextTranslations::value()[key] = entry;
				};
				log::info("Loaded {} new translations from `{}`", count, path);
			};

			// sprite sheets
			if (!value.contains("spritesheets")) log::warn(
				"It doesn't contain `spritesheets` key for sprite sheet file names"
			);
			else for (auto entry : matjson::valOrValsArr(value["spritesheets"])) {
				auto file = entry.asString().unwrapOrDefault();
				file = CCFileUtils::get()->fullPathForFilename(file.c_str(), 0).c_str();
				if (CCFileUtils::get()->isFileExist(file.c_str())) {
					CCSpriteFrameCache::get()->removeSpriteFramesFromFile(file.c_str());
					CCSpriteFrameCache::get()->addSpriteFramesWithFile(file.c_str());
					log::info("Loaded sprite frames from `{}`", file);
				} 
				else log::error("File `{}` not found", file);
			};

		} //value.size()
		else log::warn("No one `lang.json` file was found...");

		log::info(
			"Searching for any `{}` in search paths...", 
			TextTranslations::get()->getID() + ".translations.json"
		);
		for (auto shit : CCFileUtils::get()->getSearchPaths()) {
			auto searchPath = std::string(
				shit.c_str()
			);
			auto translationsFile = std::string(
				searchPath + "/" + TextTranslations::get()->getID() + ".translations.json"
			);
			if (CCFileUtils::get()->isFileExist(translationsFile)) {
				auto count = 0;
				for (auto entry : fs::readJson(translationsFile.c_str()).unwrapOrDefault()) {
					auto key = entry.getKey().value_or("");
					count += not TextTranslations::value().contains(key);
					TextTranslations::value()[key] = entry;
				};
				log::info("Loaded {} new translations from `{}`", count, translationsFile);
			}
		};

		return LoadingLayer::init(penis);
	}
};