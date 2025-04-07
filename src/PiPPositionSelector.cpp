#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/modify/CCLayer.hpp>

using namespace geode::prelude;

class PiPPositionSelector : public CCLayer, public SettingValue {
protected:
    CCSprite* m_preview = nullptr;
    CCSprite* m_pipImage = nullptr;
    CCLayerColor* m_bg = nullptr;
    bool m_isDragging = false;
    CCPoint m_dragOffset;
    float m_scale = 1.0f;
    
public:
    static PiPPositionSelector* create() {
        auto ret = new PiPPositionSelector();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
    
    bool init() override {
        if (!CCLayer::init()) return false;
        
        this->setTouchEnabled(true);
        this->setTouchMode(kCCTouchesOneByOne);
        
        // Create dark background
        m_bg = CCLayerColor::create({0, 0, 0, 100});
        m_bg->setContentSize(CCSizeMake(300, 200));
        this->addChild(m_bg);
        
        // Create preview background (simulating game background)
        auto previewBg = CCLayerColor::create({100, 150, 255, 255});  // Light blue background
        previewBg->setContentSize(CCSizeMake(300, 200));
        this->addChild(previewBg);
        
        // Add some visual elements to simulate a level
        auto ground = CCLayerColor::create({50, 50, 50, 255});  // Dark gray ground
        ground->setContentSize(CCSizeMake(300, 60));
        ground->setPosition(0, 0);
        this->addChild(ground);
        
        // Create PiP preview
        auto* mod = Mod::get();
        std::filesystem::path imagePath;
        
        if (mod->getSettingValue<bool>("pip-use-custom-image")) {
            imagePath = mod->getSettingValue<std::string>("pip-image-path");
        }
        
        if (imagePath.empty()) {
            imagePath = mod->getResourcesDir() / "death.png";
        }
        
        auto fileResult = geode::utils::file::readBinary(imagePath.string());
        if (fileResult.isOk()) {
            auto& fileData = fileResult.unwrap();
            auto* image = new CCImage();
            if (image->initWithImageData(
                static_cast<void*>(const_cast<uint8_t*>(fileData.data())),
                static_cast<int>(fileData.size())
            )) {
                auto* texture = new CCTexture2D();
                if (texture->initWithImage(image)) {
                    m_pipImage = CCSprite::createWithTexture(texture);
                    texture->release();
                }
            }
            image->release();
        }
        
        if (m_pipImage) {
            // Calculate initial size (20% of preview width)
            float pipSize = mod->getSettingValue<int>("pip-size") / 100.0f;
            float sizeMultiplier = mod->getSettingValue<float>("pip-size-multiplier");
            CCSize originalSize = m_pipImage->getContentSize();
            m_scale = (300.0f * pipSize * sizeMultiplier) / originalSize.width;
            m_pipImage->setScale(m_scale);
            
            // Set initial position based on settings
            float padding = static_cast<float>(mod->getSettingValue<int>("pip-padding"));
            CCPoint pos;
            
            if (mod->getSettingValue<bool>("has-custom-position")) {
                float normalizedX = mod->getSettingValue<float>("pip-position-x");
                float normalizedY = mod->getSettingValue<float>("pip-position-y");
                pos = ccp(normalizedX * 300.0f, normalizedY * 200.0f);
            } else {
                switch (mod->getSettingValue<int>("pip-position")) {
                    case 0:  // Top Right
                        pos = ccp(300 - (originalSize.width * m_scale)/2 - padding, 
                                200 - (originalSize.height * m_scale)/2 - padding);
                        break;
                    case 1:  // Top Left
                        pos = ccp((originalSize.width * m_scale)/2 + padding, 
                                200 - (originalSize.height * m_scale)/2 - padding);
                        break;
                    case 2:  // Bottom Right
                        pos = ccp(300 - (originalSize.width * m_scale)/2 - padding, 
                                (originalSize.height * m_scale)/2 + padding);
                        break;
                    case 3:  // Bottom Left
                        pos = ccp((originalSize.width * m_scale)/2 + padding, 
                                (originalSize.height * m_scale)/2 + padding);
                        break;
                    default:
                        pos = ccp(300 - (originalSize.width * m_scale)/2 - padding, 
                                200 - (originalSize.height * m_scale)/2 - padding);
                }
            }
            
            m_pipImage->setPosition(pos);
            this->addChild(m_pipImage);
            
            // Add instructions
            auto label = CCLabelBMFont::create(
                "Drag to position\nSize can be adjusted in settings",
                "bigFont.fnt"
            );
            label->setScale(0.4f);
            label->setPosition(ccp(150, 180));
            this->addChild(label);
        }
        
        return true;
    }
    
    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!m_pipImage) return false;
        
        auto touchLocation = touch->getLocation();
        touchLocation = this->convertToNodeSpace(touchLocation);
        
        auto bounds = m_pipImage->boundingBox();
        bounds.origin.x -= 10;
        bounds.origin.y -= 10;
        bounds.size.width += 20;
        bounds.size.height += 20;
        
        if (bounds.containsPoint(touchLocation)) {
            m_isDragging = true;
            m_dragOffset = ccpSub(m_pipImage->getPosition(), touchLocation);
            return true;
        }
        
        return false;
    }
    
    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!m_isDragging || !m_pipImage) return;
        
        auto touchLocation = touch->getLocation();
        touchLocation = this->convertToNodeSpace(touchLocation);
        auto newPos = ccpAdd(touchLocation, m_dragOffset);
        
        // Keep within bounds
        CCSize size = m_pipImage->getContentSize();
        float padding = static_cast<float>(Mod::get()->getSettingValue<int>("pip-padding"));
        
        float halfWidth = (size.width * m_scale) / 2;
        float halfHeight = (size.height * m_scale) / 2;
        
        newPos.x = std::max(halfWidth + padding, 
                   std::min(newPos.x, 300.0f - halfWidth - padding));
        newPos.y = std::max(halfHeight + padding, 
                   std::min(newPos.y, 200.0f - halfHeight - padding));
        
        m_pipImage->setPosition(newPos);
        
        // Save position
        auto* mod = Mod::get();
        if (mod) {
            float normalizedX = newPos.x / 300.0f;
            float normalizedY = newPos.y / 200.0f;
            
            mod->setSettingValue<float>("pip-position-x", normalizedX);
            mod->setSettingValue<float>("pip-position-y", normalizedY);
            mod->setSettingValue<bool>("has-custom-position", true);
        }
    }
    
    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_isDragging = false;
    }
    
    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        m_isDragging = false;
    }
};

class $modify(CCLayer) {
    bool init() {
        if (!CCLayer::init()) return false;
        
        // Register the custom widget
        Mod::get()->addCustomSetting("pip-position-selector", [](auto) {
            return PiPPositionSelector::create();
        });
        
        return true;
    }
}; 
