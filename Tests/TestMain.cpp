#include <JuceHeader.h>
#include <iostream>
int runProcessorTests();int runDspTests();int runEditorTests();int renderEditors(const juce::File&,bool animation);
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc==3&&(juce::String(argv[1])=="--render"||juce::String(argv[1])=="--render-animation"))
        return renderEditors(juce::File(juce::String(argv[2])),juce::String(argv[1])=="--render-animation");
    const int failed=runProcessorTests()+runDspTests()+runEditorTests();
    std::cout<<failed<<" failed\n";return failed?1:0;
}
