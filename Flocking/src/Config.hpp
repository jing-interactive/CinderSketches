//
//  Config.hpp
//  Flocking
//
//  Created by Yi-Wen LIN on 06/04/2020.
//

#ifndef Config_hpp
#define Config_hpp

#include <stdio.h>

#endif /* Config_hpp */




class Config {
public:
static Config& getInstance()
{
    static Config    instance;
                        
    return instance;
}
    int NUM_PARTICLES = 80;
private:
    Config() {
        
    }
    
    Config(Config const&);              // Don't Implement
    void operator=(Config const&); // Don't implement
};
