#pragma once

#include <QString>

namespace afs {

struct ComponentDefinition {
    QString id;
    QString name;
    bool operator==(const ComponentDefinition&) const = default;
};

inline constexpr auto DefaultComponentId = "component-1";
inline constexpr auto DefaultComponentName = "目标组分";

} // namespace afs
