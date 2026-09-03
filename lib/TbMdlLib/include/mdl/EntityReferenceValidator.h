/* Copyright (C) 2026 TrenchBroom Authors */
#pragma once

#include "mdl/Validator.h"

namespace tb::mdl
{
class Map;

class EntityReferenceValidator : public Validator
{
private:
  Map& m_map;

public:
  explicit EntityReferenceValidator(Map& map);

private:
  void doValidate(EntityNodeBase& entityNode, std::vector<std::unique_ptr<Issue>>& issues)
    const override;
};
} // namespace tb::mdl
