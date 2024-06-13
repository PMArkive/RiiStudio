/*!
 * @file
 * @brief Implementations for data blocks.
 */

#include "node.hxx"
#include "oishii/interfaces.hxx"

#include <fstream>

namespace oishii {

std::expected<void, std::string>
Node::gatherChildren([[maybe_unused]] NodeDelegate& mOut) const {
  return {};
}
std::expected<void, std::string>
Node::getChildren(std::vector<std::unique_ptr<Node>>& mOut) const {
  mOut.clear();
  NodeDelegate del{mOut};
  auto result = gatherChildren(del);

  if (!mOut.empty() && mLinkingRestriction.Leaf) {
    mOut.clear();
    printf("Leaf node %s attempted to supply children.\n", getId().c_str());
  }

  return result;
}

} // namespace oishii
