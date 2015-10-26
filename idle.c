//
//  idle.c
//  yalnix
//
//  Created by Wenzhe Jiang on 3/22/15.
//  Copyright (c) 2015 Wenzhe Jiang. All rights reserved.
//

#include <stdio.h>
#include "kernel.h"
int main() {
    int i;
    fflush(stdout);
    while (1){ 
        fflush(stdout);
        Pause();
    }
    return 0;
}
