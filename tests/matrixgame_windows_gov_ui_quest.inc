// Test input only: find a visible original dialog row and send mouse events.
// Neither quest selection nor battle outcome is supplied by this fixture.
auto click_gov_choice = [&](auto expected, std::int32_t* value) -> bool {
 auto* panel=pas::checked_cast<GI_PanelScrollBar::TPanelScrollBarGI*>(gov->GetByName(u"TalkPA"sv));
 if(!panel || !panel->Active) return false;
 for(auto* row=panel->FirstChild;row;row=row->NextSibling) {
  if(row->UserValue>=0) continue;
  auto* choice=srhd_awa::platform::e2e5_pointer_payload::Resolve<fTalk::TfTalkA*>(gov,row->UserValue);
  if(!choice || choice->Callback!=expected) continue;
  const int left=std::max(row->HitTestBounds.Left,panel->HitTestBounds.Left);
  const int top=std::max(row->HitTestBounds.Top,panel->HitTestBounds.Top);
  const int right=std::min(row->HitTestBounds.Right,panel->HitTestBounds.Right);
  const int bottom=std::min(row->HitTestBounds.Bottom,panel->HitTestBounds.Bottom);
  if(left>=right || top>=bottom) return false;
  if(value) *value=choice->Value;
  auto* target=native_input::CurrentUiTarget();assert(target);
  const int x=left+(right-left)/2,y=top+(bottom-top)/2;
  target->Move(x,y);
  target->Mouse(native_input::Button::Down,1,x,y);
  target->Mouse(native_input::Button::Up,1,x,y);
  return true;
 }
 return false;
};
