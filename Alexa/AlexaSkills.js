const https = require('https');

const API_URL = "https://maker.ifttt.com/trigger/YOUREVENTNAME/json/with/key/YOURKEY;

const HelloWorldIntentHandler = {
    canHandle(handlerInput) {
        return Alexa.getRequestType(handlerInput.requestEnvelope) === 'IntentRequest'
            && Alexa.getIntentName(handlerInput.requestEnvelope) === 'HelloWorldIntent';
    },
    async handle(handlerInput) {
        const component = handlerInput.requestEnvelope.request.intent.slots.component.value;
        await sendToArduino(component);
        const speechText = `Picking ${component}!`;
        return handlerInput.responseBuilder
            .speak(speechText)
            .getResponse();
    }
};

async function sendToArduino(componentName) {
    console.log(`inside function call`);
    return new Promise((resolve, reject) => {
        const options = {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        };
        const req = https.request(API_URL, options, (res) => {
            res.on('data', () => resolve());
        });
        req.on('error', reject);
        req.write(JSON.stringify({ component: componentName }));
        req.end();
    });
}