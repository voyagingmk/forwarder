const jsface = require('jsface');


const UniqIDGenerator = jsface.Class({
    constructor: function() {},
    reset() {
        this.count = 0;
        this.recycled = [];
    },
    getNewID() {
        if (this.count > 10000) {
            if (this.recycled.length > 0 && this.recycled[0] > 0) {
                const id = this.recycled.shift();
                return id;
            }
        }
        this.count++;
        return this.count;
    },
    recycleID(id) {
        recycled.push(id);
    }
});

module.exports = {
    UniqIDGenerator,
};