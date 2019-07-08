#include "generator/regions/admin_suburbs_marker.hpp"

#include "generator/regions/region.hpp"

namespace generator
{
namespace regions
{
void AdminSuburbsMarker::MarkSuburbs(Node::Ptr & tree)
{
  auto const & region = tree->GetData();
  if (region.GetLevel() == PlaceLevel::Locality)
  {
    MarkLocality(tree);
    return;
  }

  for (auto & subtree : tree->GetChildren())
    MarkSuburbs(subtree);
}

void AdminSuburbsMarker::MarkLocality(Node::Ptr & tree)
{
  ASSERT_EQUAL(tree->GetData().GetLevel(), PlaceLevel::Locality, ());
  for (auto & subtree : tree->GetChildren())
    MarkSuburbsInLocality(subtree);
}

void AdminSuburbsMarker::MarkSuburbsInLocality(Node::Ptr & tree)
{
  auto & region = tree->GetData();
  if (region.GetLevel() == PlaceLevel::Locality)
  {
    MarkLocality(tree);
    return;
  }

  if (region.GetAdminLevel() != AdminLevel::Unknown)
    region.SetLevel(PlaceLevel::Suburb);

  for (auto & subtree : tree->GetChildren())
    MarkUnderLocalityAsSublocalities(subtree);
}

void AdminSuburbsMarker::MarkUnderLocalityAsSublocalities(Node::Ptr & tree)
{
  auto & region = tree->GetData();
  auto const level = region.GetLevel();
  if (level == PlaceLevel::Locality)
  {
    MarkLocality(tree);
    return;
  }

  if (level == PlaceLevel::Suburb)
    region.SetLevel(PlaceLevel::Sublocality);
  else if (level == PlaceLevel::Unknown && region.GetAdminLevel() != AdminLevel::Unknown)
    region.SetLevel(PlaceLevel::Sublocality);

  for (auto & subtree : tree->GetChildren())
    MarkUnderLocalityAsSublocalities(subtree);
}
}  // namespace regions
}  // namespace generator
