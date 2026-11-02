"""Native low32 localization seed for original government revolutions."""
from matrixgame_campaign_cleanup_overrides import replace_body

def apply(name,text):
 if name!='aPlanet.cpp':return text
 return replace_body(text,'    void TPlanet::TriggerGovernmentRevolution() {','    void TPlanet::TryTriggerEconomicEvent() {',
  'aGalaxy::Galaxy->CurrentTurn / 10 * static_cast<std::int32_t>(GenerationSeed)',
  'aGalaxy::Galaxy->CurrentTurn / 10 * GenerationSeed')
