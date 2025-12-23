// WebMIDI safety guard - ensures globalThis.__libreMidi_access is properly initialized
Module['preRun'].push(function() {
    // Ensure globalThis.__libreMidi_access exists to prevent "Cannot read properties of undefined" errors
    if (typeof globalThis === 'undefined') {
        globalThis = window;
    }
    
    // Initialize with safe defaults to prevent race conditions
    globalThis.__libreMidi_access = {
        inputs: new Map(),
        outputs: new Map()
    };
    
    console.log('WebMIDI safety guard initialized');
});

// Override _arrayToHeap to safely handle undefined inputs
Module['onRuntimeInitialized'] = function() {
    Module['midiInitialized'] = true;
    console.log('Module initialized');
    
    // Wait for _arrayToHeap to be available, then override it
    setTimeout(function() {
        if (typeof _arrayToHeap !== 'undefined') {
            const originalArrayToHeap = _arrayToHeap;
            _arrayToHeap = function(typedArray) {
                // Safety check: if typedArray is undefined or null, return null
                if (!typedArray) {
                    console.warn('MIDI: _arrayToHeap called with undefined/null - skipping');
                    return null;
                }
                
                // Safety check: ensure typedArray has required properties
                if (typeof typedArray.length !== 'number' || typeof typedArray.BYTES_PER_ELEMENT !== 'number') {
                    console.warn('MIDI: _arrayToHeap called with invalid typedArray - skipping');
                    return null;
                }
                
                // Safety check: ensure typedArray has a buffer
                if (!typedArray.buffer) {
                    console.warn('MIDI: _arrayToHeap called with typedArray without buffer - skipping');
                    return null;
                }
                
                // Call original function
                try {
                    return originalArrayToHeap.call(this, typedArray);
                } catch (error) {
                    console.error('MIDI: Error in _arrayToHeap:', error);
                    return null;
                }
            };
            console.log('_arrayToHeap safety override installed');
        } else {
            console.warn('Could not install _arrayToHeap safety override - function not available');
        }
    }, 200); // Delay to ensure all functions are initialized
    
    // Also override _freeArray to handle null inputs safely
    setTimeout(function() {
        if (typeof _freeArray !== 'undefined') {
            const originalFreeArray = _freeArray;
            _freeArray = function(heapBytes) {
                // Safety check: if heapBytes is null or undefined, skip freeing
                if (!heapBytes) {
                    return;
                }
                
                // Call original function
                try {
                    return originalFreeArray.call(this, heapBytes);
                } catch (error) {
                    console.error('MIDI: Error in _freeArray:', error);
                }
            };
            console.log('_freeArray safety override installed');
        }
    }, 200);
};

// WebMIDI access handler
Module['onRuntimeInitialized'] = function() {
    Module['midiInitialized'] = true;
    console.log('Module initialized');
    
    // Patch global WebMIDI access to add safety checks
    setTimeout(function() {
        if (typeof globalThis !== 'undefined' && globalThis.__libreMidi_access) {
            // Patch existing MIDI inputs to have safe message handlers
            const inputs = globalThis.__libreMidi_access.inputs;
            if (inputs && inputs.values) {
                const inputArray = inputs.values();
                for (const input of inputArray) {
                    if (input) {
                        const originalOnMidimessage = input.onmidimessage;
                        input.onmidimessage = function(message) {
                            try {
                                // Safety check for message data
                                if (!message || !message.data || !(message.data instanceof Uint8Array) || message.data.length === 0) {
                                    console.warn('MIDI: Invalid or empty message data - skipping');
                                    return;
                                }
                                
                                // Call original handler if it exists
                                if (originalOnMidimessage && typeof originalOnMidimessage === 'function') {
                                    originalOnMidimessage.call(this, message);
                                }
                            } catch (error) {
                                console.error('MIDI: Error in safe message handler:', error);
                            }
                        };
                    }
                }
            }
        }
    }, 100); // Small delay to ensure WebMIDI is initialized
};