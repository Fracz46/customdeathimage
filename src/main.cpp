#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/file.hpp>
#include <cocos2d.h>
#include <filesystem>
#include <random>
using namespace geode::prelude;

struct MemeAsset {
    std::filesystem::path imagePath;
    std::filesystem::path soundPath;
};

std::vector<std::filesystem::path> getImagesFromFolder(const std::filesystem::path& folderPath) {
    std::vector<std::filesystem::path> images;
    
    if (!std::filesystem::exists(folderPath)) {
        log::error("Folder does not exist: {}", folderPath.string());
        return images;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png") {
                images.push_back(entry.path());
            }
        }
    } catch (const std::exception& e) {
        log::error("Error reading folder: {}", e.what());
    }
    
    return images;
}

std::filesystem::path getRandomImage(const std::vector<std::filesystem::path>& images) {
    if (images.empty()) return "";
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, images.size() - 1);
    
    return images[dis(gen)];
}

std::filesystem::path findMatchingSoundFile(const std::filesystem::path& imagePath) {
    auto folder = imagePath.parent_path();
    auto stem = imagePath.stem().string();
    
    std::filesystem::path soundPath = folder / (stem + ".ogg");
    if (std::filesystem::exists(soundPath)) {
        return soundPath;
    }
    
    soundPath = folder / (stem + ".mp3");
    if (std::filesystem::exists(soundPath)) {
        return soundPath;
    }
    
    return "";
}

std::vector<MemeAsset> getMemeAssets(const std::filesystem::path& memesPath) {
    std::vector<MemeAsset> memes;
    std::map<std::string, MemeAsset> memeMap;
    
    if (!std::filesystem::exists(memesPath)) {
        log::error("Memes folder does not exist: {}", memesPath.string());
        return memes;
    }
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(memesPath)) {
            if (!entry.is_regular_file()) continue;
            
            auto ext = entry.path().extension().string();
            auto stem = entry.path().stem().string();
            
            if (ext == ".png") {
                memeMap[stem].imagePath = entry.path();
            }
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(memesPath)) {
            if (!entry.is_regular_file()) continue;
            
            auto ext = entry.path().extension().string();
            auto stem = entry.path().stem().string();
            
            if (ext == ".mp3" || ext == ".ogg") {
                if (memeMap.contains(stem) && !memeMap[stem].imagePath.empty()) {
                    memeMap[stem].soundPath = entry.path();
                    memes.push_back(memeMap[stem]);
                }
            }
        }
    } catch (const std::exception& e) {
        log::error("Error reading memes folder: {}", e.what());
    }
    
    return memes;
}

MemeAsset getRandomMeme(const std::vector<MemeAsset>& memes) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    if (memes.empty()) return MemeAsset{};
    
    std::uniform_int_distribution<> dis(0, memes.size() - 1);
    return memes[dis(gen)];
}

class $modify(PlayerObject) {
    struct Fields {
        float soundStopTime = 0.0f;
        CCSprite* currentDeathImage = nullptr;
        std::string currentSoundPath;
    };

    void playerDestroyed(bool p0) {
        stopSound();
        PlayerObject::playerDestroyed(p0);
        
        auto* mod = Mod::get();
        if (!mod || !mod->getSettingValue<bool>("enabled")) return;

        auto* playLayer = PlayLayer::get();
        if (!playLayer) return;

        // Check if we're in dual mode and this is the second player
        if (playLayer->m_player2) {  // If player2 exists, we're in dual mode
            if (this != playLayer->m_player1) {
                return; // Skip death effects for second player in dual mode
            }
        }

        // Check if we're in Globed multiplayer and this is not the local player
        if (auto* globedMod = Loader::get()->getLoadedMod("geode.globed")) {
            // For Globed, we can check if this player is not the main player
            if (this != playLayer->m_player1) {
                return; // Skip death effects for other players in multiplayer
            }
        }

        if (!mod->getSettingValue<bool>("show-in-practice") && playLayer->m_isPracticeMode) return;

        if (mod->getSettingValue<bool>("meme-mode")) {
            auto memesPath = mod->getResourcesDir() / "memes";
            auto memes = getMemeAssets(memesPath);
            
            if (memes.empty()) {
                log::error("No meme assets found in: {}", memesPath.string());
                return;
            }
            
            auto meme = getRandomMeme(memes);
            if (meme.imagePath.empty()) {
                log::error("Failed to get random meme");
                return;
            }
            
            if (!meme.soundPath.empty()) {
                playSound(meme.soundPath);
            }
            
            displayImage(meme.imagePath);
            return;
        }
        
        if (mod->getSettingValue<bool>("use-custom-sound")) {
            auto soundPath = mod->getSettingValue<std::string>("custom-sound-path");
            if (!soundPath.empty()) {
                playSound(soundPath);
            }
        }
        
        int minPercentage = mod->getSettingValue<int>("min-percentage");
        int currentPercentage = playLayer->getCurrentPercentInt();
        
        if (minPercentage > 0 && currentPercentage < minPercentage) {
            log::info("Current percentage {} is below minimum {}, not showing death image", 
                     currentPercentage, minPercentage);
            return;
        }

        std::filesystem::path imagePath;
        
        if (mod->getSettingValue<bool>("use-custom-image")) {
            if (mod->getSettingValue<bool>("use-folder")) {
                auto folderPath = mod->getSettingValue<std::string>("custom-folder-path");
                if (folderPath.empty()) {
                    log::error("Custom folder enabled but no path specified");
                    return;
                }
                
                auto images = getImagesFromFolder(folderPath);
                if (images.empty()) {
                    log::error("No PNG images found in folder: {}", folderPath);
                    return;
                }
                
                imagePath = getRandomImage(images);
                log::info("Using random image from folder: {}", imagePath.string());
                
                if (mod->getSettingValue<bool>("use-image-specific-sounds") && 
                    !mod->getSettingValue<bool>("use-custom-sound")) {
                    auto soundPath = findMatchingSoundFile(imagePath);
                    if (!soundPath.empty()) {
                        playSound(soundPath);
                    }
                }
            } else {
                auto customPath = mod->getSettingValue<std::string>("custom-image-path");
                if (customPath.empty()) {
                    log::error("Custom image enabled but no path specified");
                    return;
                }
                imagePath = customPath;
                log::info("Using custom image from: {}", imagePath.string());
                
                if (mod->getSettingValue<bool>("use-image-specific-sounds") && 
                    !mod->getSettingValue<bool>("use-custom-sound")) {
                    auto soundPath = findMatchingSoundFile(imagePath);
                    if (!soundPath.empty()) {
                        playSound(soundPath);
                    }
                }
            }
        } else {
            imagePath = mod->getResourcesDir() / "death.png";
            log::info("Using default image from: {}", imagePath.string());
            
            if (!mod->getSettingValue<bool>("use-custom-sound")) {
                auto soundPath = mod->getResourcesDir() / "death.ogg";
                playSound(soundPath);
            }
        }
        
        displayImage(imagePath);
    }
    
    void displayImage(const std::filesystem::path& imagePath) {
        auto* director = CCDirector::sharedDirector();
        CCSize winSize = director->getWinSize();
        
        auto fileResult = geode::utils::file::readBinary(imagePath.string());
        if (!fileResult.isOk()) {
            log::error("Failed to read file data: {}", fileResult.unwrapErr());
            return;
        }
        
        auto& fileData = fileResult.unwrap();
        log::info("Read {} bytes of image data", fileData.size());
        
        auto* image = new CCImage();
        if (!image->initWithImageData(
            static_cast<void*>(const_cast<uint8_t*>(fileData.data())),
            static_cast<int>(fileData.size())
        )) {
            log::error("Failed to create image from data");
            image->release();
            return;
        }
        
        log::info("Image dimensions: {} x {}", image->getWidth(), image->getHeight());
        
        auto* texture = new CCTexture2D();
        if (!texture->initWithImage(image)) {
            log::error("Failed to create texture from image");
            image->release();
            texture->release();
            return;
        }
        
        image->release();
        
        auto deathImage = CCSprite::createWithTexture(texture);
        texture->release();
        
        if (!deathImage) {
            log::error("Failed to create sprite from texture");
            return;
        }
        
        CCSize size = deathImage->getContentSize();
        log::info("Sprite size: {} x {}", static_cast<int>(size.width), static_cast<int>(size.height));
        
        float scaleX = winSize.width / size.width;
        float scaleY = winSize.height / size.height;
        float scale = std::max(scaleX, scaleY);
        
        deathImage->setScale(scale);
            deathImage->setAnchorPoint(ccp(0.5f, 0.5f));
            deathImage->setPosition(ccp(winSize.width / 2, winSize.height / 2));
        
        ccBlendFunc blend;
        blend.src = GL_SRC_ALPHA;
        blend.dst = GL_ONE_MINUS_SRC_ALPHA;
        deathImage->setBlendFunc(blend);
        
        deathImage->setID("death-image");
        
        auto* playLayer = PlayLayer::get();
        if (playLayer) {
            playLayer->addChild(deathImage, 1024);
            
            float duration = Mod::get()->getSettingValue<float>("death-duration");
            
            bool isDefaultDeath = (imagePath == Mod::get()->getResourcesDir() / "death.png");
            
            if (isDefaultDeath) {
                auto* blackOverlay = CCLayerColor::create(ccc4(0, 0, 0, 255));
                blackOverlay->setID("black-overlay");
                playLayer->addChild(blackOverlay, 1023);
                
                deathImage->setScale(scale * 0.1f);
                deathImage->setOpacity(0);
                blackOverlay->setOpacity(0);
                
                auto stutterDelay = 0.05f;
                auto numStutters = 3;
                
                auto scaleUp = CCScaleTo::create(0.1f, scale * 1.2f);
                auto fadeIn = CCFadeIn::create(0.1f);
                auto scaleNormal = CCScaleTo::create(0.05f, scale);
                
                auto* stutterSeq = CCArray::create();
                for(int i = 0; i < numStutters; i++) {
                    stutterSeq->addObject(CCFadeTo::create(0.05f, 255));
                    stutterSeq->addObject(CCFadeTo::create(0.05f, 0));
                }
                
                auto delay = CCDelayTime::create(duration - 0.35f - (stutterDelay * numStutters * 2));
                auto fadeOut = CCFadeOut::create(0.2f);
                
                blackOverlay->runAction(CCSequence::create(
                    CCDelayTime::create(0.15f),
                    CCSequence::create(stutterSeq),
                    CCFadeOut::create(0.1f),
                    nullptr
                ));
                
                deathImage->runAction(CCSpawn::create(scaleUp, fadeIn, nullptr));
            deathImage->runAction(CCSequence::create(
                    CCDelayTime::create(0.1f),
                    scaleNormal,
                    delay,
                    fadeOut,
                nullptr
            ));
                
                auto jumpscareSound = Mod::get()->getResourcesDir() / "jumpsc.mp3";
                if (std::filesystem::exists(jumpscareSound)) {
                    playSound(jumpscareSound);
                }
            } else {
                deathImage->setOpacity(255);
                auto fadeOut = CCFadeOut::create(0.2f);
                
                auto fadeSequence = CCSequence::create(
                    CCDelayTime::create(duration - 0.2f),
                    fadeOut,
                    nullptr
                );
                deathImage->runAction(fadeSequence);
            }
        }
    }

    void update(float dt) {
        PlayerObject::update(dt);
        m_fields->soundStopTime = 0.0f;
    }

    void playSound(const std::filesystem::path& soundPath) {
        if (soundPath.empty()) return;
        
        auto* engine = FMODAudioEngine::sharedEngine();
        if (!engine) return;

        auto* mod = Mod::get();
        if (!mod) return;

        m_fields->currentSoundPath = soundPath.string();
        
        FMOD::Sound* sound = nullptr;
        FMOD::Channel* channel = nullptr;
        
        if (engine->m_system->createStream(
            soundPath.string().c_str(),
            FMOD_DEFAULT,
            nullptr,
            &sound
        ) == FMOD_OK) {
            if (sound && engine->m_system->playSound(
                sound,
                nullptr,
                false,
                &channel
            ) == FMOD_OK) {
                if (channel) {
                    float volume = mod->getSettingValue<float>("sound-volume");
                    channel->setVolume(volume);
                }
            }
        }
    }

    void stopSound() {
        if (!m_fields->currentSoundPath.empty()) {
            auto* engine = FMODAudioEngine::sharedEngine();
            if (engine) {
                engine->stopAllEffects();
            }
            m_fields->currentSoundPath.clear();
        }
    }

    void cleanupDeath() {
        stopSound();
        if (m_fields->currentDeathImage) {
            m_fields->currentDeathImage->removeFromParent();
            m_fields->currentDeathImage = nullptr;
        }
    }
};

class $modify(PlayLayer) {
    struct Fields {
        CCSprite* miniImage = nullptr;
        bool isDragging = false;
        CCPoint dragOffset;
        CCSprite* levelPreview = nullptr;
        CCLayerColor* previewBg = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        
        auto* mod = Mod::get();
        if (!mod || !mod->getSettingValue<bool>("enabled")) return true;
        
        auto dispatcher = CCDirector::sharedDirector()->getKeyboardDispatcher();
        if (dispatcher) {
            dispatcher->addDelegate(this);
        }
        
        setupPiP();
        return true;
    }

    void keyDown(cocos2d::enumKeyCodes key) {
        PlayLayer::keyDown(key);
        
        if (key == cocos2d::KEY_O) {
            showPositioningPreview();
        }
    }

    void showPositioningPreview() {
        auto* director = CCDirector::sharedDirector();
        auto winSize = director->getWinSize();

        if (!m_fields->previewBg) {
            m_fields->previewBg = CCLayerColor::create(ccc4(0, 0, 0, 180));
            m_fields->previewBg->setContentSize(winSize);
            this->addChild(m_fields->previewBg, 1000);
        }

        if (!m_fields->levelPreview) {
            float previewWidth = winSize.width * 0.3f;
            float previewHeight = previewWidth * (winSize.height / winSize.width);

            auto renderTexture = CCRenderTexture::create(winSize.width, winSize.height);
            renderTexture->begin();
            
            auto gameLayer = this->getChildByID("game-layer");
            if (gameLayer) {
                gameLayer->visit();
            }
            
            renderTexture->end();

            m_fields->levelPreview = CCSprite::createWithTexture(renderTexture->getSprite()->getTexture());
            m_fields->levelPreview->setScale(previewWidth / winSize.width);
            m_fields->levelPreview->setPosition(ccp(winSize.width / 2, winSize.height / 2));
            m_fields->levelPreview->setOpacity(200);
            this->addChild(m_fields->levelPreview, 1001);

            auto instructions = CCLabelBMFont::create(
                "Click and drag to position the PiP window\nPress O again to save",
                "bigFont.fnt"
            );
            instructions->setScale(0.5f);
            instructions->setPosition(ccp(winSize.width / 2, winSize.height - 30));
            instructions->setZOrder(1002);
            this->addChild(instructions, 1002);

            this->setTouchEnabled(true);
            this->setTouchMode(kCCTouchesOneByOne);
            this->setTouchPriority(-1000);

            this->m_isPaused = true;
            CCDirector::sharedDirector()->pause();
        } else {
            hidePositioningPreview();
        }
    }

    void hidePositioningPreview() {
        if (m_fields->previewBg) {
            m_fields->previewBg->removeFromParent();
            m_fields->previewBg = nullptr;
        }
        if (m_fields->levelPreview) {
            m_fields->levelPreview->removeFromParent();
            m_fields->levelPreview = nullptr;
        }

        this->removeChildByID("instructions");

        this->m_isPaused = false;
        CCDirector::sharedDirector()->resume();
    }

    virtual bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!m_fields->miniImage) return false;
        
        auto touchLocation = touch->getLocation();
        auto bounds = m_fields->miniImage->boundingBox();
        
        bounds.origin.x -= 10;
        bounds.origin.y -= 10;
        bounds.size.width += 20;
        bounds.size.height += 20;
        
        if (bounds.containsPoint(touchLocation)) {
            m_fields->isDragging = true;
            m_fields->dragOffset = ccpSub(m_fields->miniImage->getPosition(), touchLocation);
            return true;
        }
        
        return false;
    }
    
    virtual void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!m_fields->isDragging || !m_fields->miniImage) return;
        
        auto touchLocation = touch->getLocation();
        auto newPos = ccpAdd(touchLocation, m_fields->dragOffset);
        
        auto* director = CCDirector::sharedDirector();
        CCSize winSize = director->getWinSize();
        CCSize size = m_fields->miniImage->getContentSize();
        float scale = m_fields->miniImage->getScale();
        float padding = static_cast<float>(Mod::get()->getSettingValue<int>("pip-padding"));
        
        float halfWidth = (size.width * scale) / 2;
        float halfHeight = (size.height * scale) / 2;
        
        newPos.x = std::max(halfWidth + padding, 
                   std::min(newPos.x, winSize.width - halfWidth - padding));
        newPos.y = std::max(halfHeight + padding, 
                   std::min(newPos.y, winSize.height - halfHeight - padding));
        
        m_fields->miniImage->setPosition(newPos);
        
        if (auto* bg = this->getChildByTag(999)) {
            bg->setPosition(newPos.x - (halfWidth * 2 + 10)/2, newPos.y - (halfHeight * 2 + 10)/2);
        }
        
        auto* mod = Mod::get();
        if (mod) {
            float normalizedX = newPos.x / winSize.width;
            float normalizedY = newPos.y / winSize.height;
            
            mod->setSettingValue<float>("pip-position-x", normalizedX);
            mod->setSettingValue<float>("pip-position-y", normalizedY);
            mod->setSettingValue<bool>("has-custom-position", true);
        }
    }
    
    virtual void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_fields->isDragging = false;
    }
    
    virtual void ccTouchCancelled(CCTouch*, CCEvent*) override {
        m_fields->isDragging = false;
    }

    void setupPiP() {
        auto* mod = Mod::get();
        if (!mod || !mod->getSettingValue<bool>("enabled") || !mod->getSettingValue<bool>("pip-mode")) return;

        std::filesystem::path imagePath;
        if (mod->getSettingValue<bool>("pip-use-custom-image")) {
            imagePath = mod->getSettingValue<std::string>("pip-image-path");
        }
        if (imagePath.empty()) {
            imagePath = mod->getResourcesDir() / "livereact.png";
        }

        auto fileResult = geode::utils::file::readBinary(imagePath.string());
        if (!fileResult.isOk()) return;

        auto& fileData = fileResult.unwrap();
        auto* image = new CCImage();
        if (!image->initWithImageData(
            static_cast<void*>(const_cast<uint8_t*>(fileData.data())),
            static_cast<int>(fileData.size())
        )) {
            image->release();
            return;
        }

        auto* texture = new CCTexture2D();
        if (!texture->initWithImage(image)) {
            image->release();
            texture->release();
            return;
        }

        m_fields->miniImage = CCSprite::createWithTexture(texture);
        texture->release();
        image->release();

        if (!m_fields->miniImage) return;

        float pipSize = mod->getSettingValue<int>("pip-size") / 100.0f;
        float sizeMultiplier = mod->getSettingValue<float>("pip-size-multiplier");
        CCSize originalSize = m_fields->miniImage->getContentSize();
        float scale = (CCDirector::sharedDirector()->getWinSize().width * pipSize * sizeMultiplier) / originalSize.width;
        m_fields->miniImage->setScale(scale);

        CCSize scaledSize = CCSizeMake(originalSize.width * scale, originalSize.height * scale);
        auto* bg = CCLayerColor::create(ccc4(0, 0, 0, 100), scaledSize.width, scaledSize.height);
        this->addChild(bg);
        this->addChild(m_fields->miniImage);

        float padding = static_cast<float>(mod->getSettingValue<int>("pip-padding"));
        CCSize winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint basePos;
        
        float offsetX = static_cast<float>(mod->getSettingValue<int>("pip-offset-x"));
        float offsetY = static_cast<float>(mod->getSettingValue<int>("pip-offset-y"));
        
        switch (mod->getSettingValue<int>("pip-position")) {
            case 0:
                basePos = ccp(winSize.width - scaledSize.width/2 - padding + offsetX, 
                             winSize.height - scaledSize.height/2 - padding - offsetY);
                break;
            case 1:
                basePos = ccp(scaledSize.width/2 + padding + offsetX, 
                             winSize.height - scaledSize.height/2 - padding - offsetY);
                break;
            case 2:
                basePos = ccp(winSize.width - scaledSize.width/2 - padding + offsetX, 
                             scaledSize.height/2 + padding + offsetY);
                break;
            case 3:
                basePos = ccp(scaledSize.width/2 + padding + offsetX, 
                             scaledSize.height/2 + padding + offsetY);
                break;
        }

        basePos.x = std::max(scaledSize.width/2 + padding, 
                     std::min(basePos.x, winSize.width - scaledSize.width/2 - padding));
        basePos.y = std::max(scaledSize.height/2 + padding, 
                     std::min(basePos.y, winSize.height - scaledSize.height/2 - padding));

        m_fields->miniImage->setPosition(basePos);
        bg->setPosition(basePos.x - scaledSize.width/2, basePos.y - scaledSize.height/2);
    }

    void onEnter() {
        PlayLayer::onEnter();
        setupPiP();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        setupPiP();
    }
};
