// Products whose flash layer has no strokes must get the same hash (ETag) as
// the same product without the layer (BRAINSTORM-3501)

#define BOOST_TEST_MODULE EmptyLayerHash
#include "Hash.h"
#include <boost/test/unit_test.hpp>
#include <list>
#include <memory>
#include <vector>

using SmartMet::Plugin::Dali::empty_hash;
using SmartMet::Plugin::Dali::State;

namespace
{
// A layer with a fixed hash value; the containers only pass the State along
struct FakeLayer
{
  std::size_t hash;
  std::size_t hash_value(const State& /* theState */) const { return hash; }
};

using LayerList = std::list<std::shared_ptr<FakeLayer>>;

const State& state()
{
  // Never dereferenced by the hash helpers under test
  alignas(State) static const unsigned char storage[sizeof(State)] = {};
  return *reinterpret_cast<const State*>(storage);
}

std::size_t hash(const LayerList& layers)
{
  return SmartMet::Plugin::Dali::hash_value(layers, state());
}

std::shared_ptr<FakeLayer> layer(std::size_t value)
{
  return std::make_shared<FakeLayer>(FakeLayer{value});
}
}  // namespace

BOOST_AUTO_TEST_CASE(empty_flash_layer_on_top_of_radar)
{
  auto radar = layer(12345);
  auto flash = layer(empty_hash);
  BOOST_CHECK_EQUAL(hash({radar, flash}), hash({radar}));
  BOOST_CHECK_EQUAL(hash({flash, radar}), hash({radar}));
  BOOST_CHECK_EQUAL(hash({flash}), hash({}));
}

BOOST_AUTO_TEST_CASE(flash_layer_with_strokes_changes_the_hash)
{
  auto radar = layer(12345);
  BOOST_CHECK_NE(hash({radar, layer(777)}), hash({radar}));
  BOOST_CHECK_NE(hash({radar, layer(777)}), hash({radar, layer(778)}));
}

BOOST_AUTO_TEST_CASE(empty_layer_inside_a_group)
{
  // A shared pointer to an empty layer stays empty, so a group (list) of only empty
  // layers hashes like an empty group
  auto flash = layer(empty_hash);
  BOOST_CHECK_EQUAL(SmartMet::Plugin::Dali::hash_value(flash, state()), empty_hash);

  std::vector<std::shared_ptr<FakeLayer>> with{layer(1), flash, layer(2)};
  std::vector<std::shared_ptr<FakeLayer>> without{layer(1), layer(2)};
  BOOST_CHECK_EQUAL(SmartMet::Plugin::Dali::hash_value(with, state()),
                    SmartMet::Plugin::Dali::hash_value(without, state()));
}
