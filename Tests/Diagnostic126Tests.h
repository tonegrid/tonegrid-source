#pragma once
#include "DiagnosticView.h"
#include "Content/Presets/PresetShare.h"
namespace diagnostic126Tests
{
inline bool run(juce::String& report)
{
    using namespace juce;bool ok=true;const auto check=[&](bool pass,const String& name){ok&=pass;report+=String(pass?"PASS ":"FAIL ")+name+"\n";};
    const String original="Driver error: C:\\Users\\Alice\\Audio\\capture.nam\nAuthorization: Bearer secret\nhttps://example.test?code=secret\napi_key=secret\n";
    const auto clean=tonegrid::diagnostics::sanitize(original,{"C:\\Users\\Alice"});
    check(!clean.contains("Alice")&&!clean.contains("secret")&&clean.contains("Driver error")&&clean.contains("[User folder]"),"diagnostics redact personal home paths and authentication details");
    String large;for(int i=0;i<80;++i)large+="Log line "+String(i)+"\n";
    const auto tail=tonegrid::diagnostics::recentLog(large);check(!tail.contains("Log line 0\n")&&tail.contains("Log line 79"),"diagnostics bound recent log to last 32 lines");
    const String text="Audio error: interface & driver / é\nEND OF TONEGRID DIAGNOSTIC\n";
    const auto uri=tonegrid::diagnostics::mailto(text);URL parsed(uri);String decoded;
    for(int i=0;i<parsed.getParameterNames().size();++i)if(parsed.getParameterNames()[i]=="body")decoded=parsed.getParameterValues()[i];
    check(uri.startsWith("mailto:christian@tonegrid.net?")&&decoded.replace("\r\n","\n")==text,"email contains complete escaped UTF-8 diagnostic and corrected recipient");
    check(String(tonegrid::sharing::recipient)==tonegrid::diagnostics::recipient,"preset sharing uses corrected email too");
    tonegrid::diagnostics::View view(text);int drafts=0;
    view.launchEmail=[&](const String& value){++drafts;check(value==uri,"email launcher receives the complete report");return false;};
    check(!view.openDraft()&&drafts==1,"missing mail handler falls back without claiming delivery");
    check(view.findChildWithID("diagnostic-copy")&&view.findChildWithID("diagnostic-text"),"complete report and Copy Diagnostic remain accessible");
    tonegrid::popup::Frame frame(view,false,"Send Diagnostic",320);frame.setSize(480,360);
    auto* viewport=view.findParentComponentOfClass<Viewport>();if(viewport)viewport->setViewPosition(0,view.getHeight());
    check(viewport&&viewport->getViewArea().contains(view.findChildWithID("diagnostic-copy")->getBounds())&&frame.findChildWithID("popup-close"),"small diagnostic popup reaches fallback controls and retains Close");
    return ok;
}
}
