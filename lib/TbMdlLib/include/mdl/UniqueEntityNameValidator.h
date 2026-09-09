/* Copyright (C) 2026 TrenchBroom Authors */
#pragma once

#include "mdl/Validator.h"

namespace tb::mdl
{
class Map;

class UniqueEntityNameValidator : public Validator
{
private:
  Map& m_map;

public:
  explicit UniqueEntityNameValidator(Map& map);

private:
  void doValidate(EntityNodeBase& entityNode, std::vector<std::unique_ptr<Issue>>& issues)
    const override;
};
} // namespace tb::mdl
