#pragma once

#include "ui_object.hpp"

#include <functional>
#include <string>
#include <vector>

namespace EC_BlockPar { struct TBlockParEC; }
namespace srhd_awa::package { class Package; }
namespace srhd_awa::platform::image_object { enum class Kind; }
namespace srhd_awa::platform::ui { class UiImageLeaf; }
namespace srhd_awa::platform::font_repository { class Repository; }

namespace srhd_awa::platform::ui_config {

using DepthResolver = std::function<bool(const std::string&, double*)>;

// The factory asks this interface to materialize a resource; it never reaches
// into a package, cache, or global runtime state itself.
class IUiResourceResolver {
 public:
  virtual ~IUiResourceResolver() = default;
  virtual bool LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                         const std::string& resource, const std::string& option,
                         std::string* error) = 0;
};

// Thin synchronous adapter for the already-portable Package resource path.
class PackageUiResourceResolver final : public IUiResourceResolver {
 public:
  explicit PackageUiResourceResolver(package::Package* package) : package_(package) {}
  bool LoadImage(ui::UiImageLeaf* leaf, image_object::Kind kind,
                 const std::string& resource, const std::string& option,
                 std::string* error) override;

 private:
  package::Package* package_{};
};

enum class LoadMode { Inventory, Strict };

struct LoadReport {
  std::vector<std::string> unsupported_controls;
  std::vector<std::string> skipped_events;
};

// The adapter owns no EC_BlockPar state.  UiObject remains renderable after
// the borrowed config tree has been released.
struct Context {
  EC_BlockPar::TBlockParEC* styles{};
  DepthResolver resolve_depth;
  IUiResourceResolver* resources{};
  font_repository::Repository* fonts{};
  EC_BlockPar::TBlockParEC* language{};
  std::function<std::string(const std::string&)> resolve_label_font_alias;
};

bool ApplyBaseProperties(ui::UiObject* object, EC_BlockPar::TBlockParEC* block,
                         const Context& context, std::string* error = nullptr);
bool ResolveRuntimeDepth(EC_BlockPar::TBlockParEC* depth_config, const std::string& name,
                         double* value);
// Reads direct child blocks and transfers every constructed node into parent.
// Unknown non-control grouping blocks are transparent; known controls that are
// outside M22 are rejected in Strict mode and recorded/skipped in Inventory.
bool LoadChildren(ui::UiObject* parent, EC_BlockPar::TBlockParEC* block,
                  const Context& context, LoadMode mode, LoadReport* report = nullptr,
                  std::string* error = nullptr);
// Build one already-selected Label block from a release config without
// traversing unsupported siblings or copying game configuration data.
bool LoadLabel(ui::UiObject* parent, EC_BlockPar::TBlockParEC* block,
               const Context& context, std::string* error = nullptr);

}  // namespace srhd_awa::platform::ui_config
