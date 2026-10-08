#include "ui_controls_checkpoint.hpp"

#include "aft_font.hpp"
#include "m24_checkpoint_fixture.hpp"
#include "ui_graph_button.hpp"
#include "ui_label.hpp"
#include "ui_window.hpp"
#include "ui_zone.hpp"

#include <array>
#include <memory>
#include <vector>

namespace srhd_awa::platform::ui_controls_checkpoint {
namespace {
bool Fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
std::unique_ptr<ui::UiImageLeaf> Image(const char* name, const char* resource,
                                       std::uint8_t red, std::uint8_t green,
                                       std::uint8_t blue, std::string* error) {
  auto image = std::make_unique<ui::UiImageLeaf>();
  image->SetName(name);
  const auto bytes = m24_checkpoint_fixture::Bmp(red, green, blue);
  if (!image->LoadBytes(image_object::Kind::Simple, bytes.data(), bytes.size(),
                        resource, "", error)) return {};
  return image;
}
bool Match(const ui_fingerprint::Value& value, std::uint32_t crc,
           std::uint64_t fnv, const char* message, std::string* error) {
  if (value.crc32 == crc && value.fnv64 == fnv) return true;
  return Fail(error, message);
}
}  // namespace

bool Checkpoint::Initialize(std::string* error) {
  if (initialized_) return Fail(error, "M24 checkpoint is already initialized");
  const auto aft = m24_checkpoint_fixture::Aft();
  auto font = std::make_shared<aft_font::AftFont>();
  if (!font->Load(aft.data(), aft.size(), error)) return false;
  font_ = font;
  tree_.SetRootSize({12, 10});
  window_ = tree_.Root()->AddWindow();
  window_->SetName("window"); window_->SetSize({10, 8}); window_->SetPosition({1, 1});
  window_->SetMinimumSize({10, 8}); window_->SetWorkSubRect({1, 1, 9, 7});
  constexpr struct { ui::WindowSlot slot; const char* name; } border[] = {
      {ui::WindowSlot::Left, "left"}, {ui::WindowSlot::Right, "right"},
      {ui::WindowSlot::Top, "top"}, {ui::WindowSlot::Bottom, "bottom"},
      {ui::WindowSlot::TopLeft, "top-left"}, {ui::WindowSlot::TopRight, "top-right"},
      {ui::WindowSlot::BottomLeft, "bottom-left"},
      {ui::WindowSlot::BottomRight, "bottom-right"},
      {ui::WindowSlot::Texture, "texture"}};
  for (const auto& [slot, name] : border) {
    const auto full_name = std::string("window-") + name;
    auto image = Image(full_name.c_str(), "B.BMP", 0, 0, 255, error);
    if (!image || !window_->AddBorderImage(slot, std::move(image), error)) return false;
  }
  if (!window_->FinalizeLayout(error)) return false;
  button_ = window_->AddGraphButton(); button_->SetName("button-a");
  button_->SetPosition({2, 1}); button_->SetSize({5, 6}); button_->SetDepth(20);
  auto normal = Image("a-normal", "R.BMP", 255, 0, 0, error);
  auto active = Image("a-normal-a", "G.BMP", 0, 255, 0, error);
  auto down = Image("a-down", "Y.BMP", 255, 255, 0, error);
  if (!normal || !active || !down ||
      !button_->AddStateImage(ui::GraphButtonSlot::Normal, std::move(normal), error) ||
      !button_->AddStateImage(ui::GraphButtonSlot::NormalA, std::move(active), error) ||
      !button_->AddStateImage(ui::GraphButtonSlot::Down, std::move(down), error)) return false;
  auto caption = std::make_unique<ui::UiLabelLeaf>(); caption->SetName("caption");
  caption->SetFont("fixture", font_); caption->SetTextLines({u"A"});
  caption->SetAlignX(ui::LabelAlignX::Left); caption->SetAlignY(ui::LabelAlignY::Top);
  if (!button_->AddCaption(std::move(caption), error)) return false;
  button_->SetCaptionColors({0xffff, 0x07e0, 0xffe0, 0xffff, 0xffff, 0xffff});
  button_->SetCaptionOffsets({0, 0}, {1, 0});
  if (!button_->Caption()->Prepare(error)) return false;
  auto* second = window_->AddGraphButton(); second->SetName("button-b");
  second->SetPosition({7, 2}); second->SetSize({2, 2}); second->SetDepth(10);
  auto second_image = Image("b-normal", "R.BMP", 255, 0, 0, error);
  if (!second_image || !second->AddStateImage(ui::GraphButtonSlot::Normal,
                                              std::move(second_image), error)) return false;
  auto* label = window_->AddLabel(); label->SetName("label");
  label->SetPosition({0, 0}); label->SetSize({5, 5}); label->SetDepth(5);
  label->SetFont("fixture", font_); label->SetTextLines({u"A"});
  label->SetAlignX(ui::LabelAlignX::Left); label->SetAlignY(ui::LabelAlignY::Top);
  if (!label->Prepare(error)) return false;
  auto* gi = window_->AddGIObject(); gi->SetName("gi");
  gi->SetPosition({7, 5}); gi->SetDepth(4);
  const auto gai = m24_checkpoint_fixture::Gai();
  if (!gi->LoadBytes(gai.data(), gai.size(), "GI.GAI", error)) return false;
  zone_ = window_->AddZone(); zone_->SetName("zone");
  zone_->SetPosition({3, 4}); zone_->SetSize({2, 2});
  zone_->SetZoneKind(ui::ZoneKind::Circle);
  initialized_ = true;
  return true;
}

bool Checkpoint::VerifyFixed(Evidence* evidence, std::string* error) const {
  if (!initialized_ || !evidence) return Fail(error, "M24 fixed checkpoint is unavailable");
  constexpr std::uint32_t kWindowCrc = 0x80b94d87u;
  constexpr std::uint64_t kWindowFnv = UINT64_C(0x77ccd5f317a573d6);
  constexpr std::uint32_t kZoneCrc = 0x8ae2e69eu;
  constexpr std::uint64_t kZoneFnv = UINT64_C(0xd34c2faf3f348e0f);
  constexpr std::uint32_t kTreeCrc = 0x6f65eea5u;
  constexpr std::uint64_t kTreeFnv = UINT64_C(0x11b73bf4b18ab6fb);
  constexpr std::uint32_t kGraphCrc = 0x6bff73cbu;
  constexpr std::uint64_t kGraphFnv = UINT64_C(0xe1747b752476d6d6);
  constexpr std::uint32_t kFrameCrc = 0x015d1589u;
  constexpr std::uint64_t kFrameFnv = UINT64_C(0x5523d509a3a9384d);
  const std::vector<ui::Point> points{{4, 5}, {5, 6}, {6, 6}, {4, 6}, {3, 5}, {7, 7}};
  if (!ui_controls_fingerprint::ComputeWindowLayout(*window_, &evidence->window_layout) ||
      !Match(evidence->window_layout, kWindowCrc, kWindowFnv, "M24 Window layout differs from Python oracle", error) ||
      !ui_controls_fingerprint::ComputeZoneHits(*zone_, points, &evidence->zone_hits) ||
      !Match(evidence->zone_hits, kZoneCrc, kZoneFnv, "M24 Zone hit vector differs from Python oracle", error) ||
      !ui_fingerprint::ComputeTree(*tree_.Root(), &evidence->tree, error) ||
      !Match(evidence->tree, kTreeCrc, kTreeFnv, "M24 tree differs from Python oracle", error) ||
      !ui_controls_fingerprint::ComputeGraphState(*button_, &evidence->graph_state) ||
      !Match(evidence->graph_state, kGraphCrc, kGraphFnv, "M24 GraphButton differs from Python oracle", error)) return false;
  std::vector<std::uint16_t> pixels(120, 0);
  const scene_compositor::Framebuffer frame{pixels.data(), 12, 10, 12};
  return tree_.Render(frame, error) &&
         ui_fingerprint::ComputeFramebuffer(frame, &evidence->frame, error) &&
         Match(evidence->frame, kFrameCrc, kFrameFnv,
               "M24 frame differs from Python oracle", error);
}

bool Checkpoint::StartDynamic(std::string* error) {
  if (!initialized_ || dynamic_) return Fail(error, "M24 dynamic checkpoint state is invalid");
  auto disabled = Image("a-disable", "Y.BMP", 255, 255, 0, error);
  if (!disabled || !button_->AddStateImage(ui::GraphButtonSlot::Disable,
                                            std::move(disabled), error)) return false;
  button_->SetButtonKind(ui::GraphButtonKind::FixDisable);
  dynamic_ = true;
  return true;
}

bool Checkpoint::Update(std::uint64_t now_ms, std::string* error) {
  if (!dynamic_) return Fail(error, "M24 dynamic controls are not initialized");
  const auto phase = (now_ms / 1000) % 4;
  button_->SetHovered(phase == 1);
  button_->SetDown(phase == 2);
  button_->SetDisabled(phase == 3);
  const auto delta = last_tick_ ? now_ms - last_tick_ : 0;
  last_tick_ = now_ms;
  return tree_.Update(delta, error);
}

bool Checkpoint::Render(const scene_compositor::Framebuffer& target, std::string* error) const {
  if (!initialized_) return Fail(error, "M24 controls are not initialized");
  return tree_.Render(target, error);
}

}  // namespace srhd_awa::platform::ui_controls_checkpoint
