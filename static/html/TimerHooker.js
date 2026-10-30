(function (global) {
    'use strict';

    var extraElements = [];

    function hookReplace(obj, name, factory) {
        var original = obj[name];
        obj[name] = factory(original);
    }

    function hookBefore(obj, name, beforeFn) {
        var original = obj[name];
        obj[name] = function () {
            beforeFn(original, arguments);
            return original.apply(this, arguments);
        };
    }

    function hookAfter(obj, name, afterFn) {
        var original = obj[name];
        obj[name] = function () {
            var result = original.apply(this, arguments);
            return afterFn(original, arguments, result);
        };
    }

    function hookedToString(original, replacement) {
        replacement.toString = function () {
            return original.toString();
        };
    }

    function hookDateClass(originalDate, hookedConstructor) {
        Object.getOwnPropertyNames(originalDate.prototype).forEach(function (name) {
            if (name === 'constructor') return;
            try {
                var desc = Object.getOwnPropertyDescriptor(originalDate.prototype, name);
                if (desc && typeof desc.value === 'function') {
                    hookedConstructor.prototype[name] = function () {
                        return this._innerDate[name].apply(this._innerDate, arguments);
                    };
                }
            } catch (e) { }
        });
        if (typeof Symbol !== 'undefined' && Symbol.toPrimitive) {
            var origToPrimitive = originalDate.prototype[Symbol.toPrimitive];
            if (origToPrimitive) {
                hookedConstructor.prototype[Symbol.toPrimitive] = function (hint) {
                    return origToPrimitive.call(this._innerDate, hint);
                };
            }
        }
    }

    function querySelectorAllDeep(ele, selector) {
        var elements = Array.prototype.slice.call(ele.querySelectorAll(selector) || []);
        extraElements.forEach(function (shadowRoot) {
            elements = elements.concat(
                Array.prototype.slice.call(shadowRoot.querySelectorAll(selector) || [])
            );
        });
        return elements;
    }

    function isInIframe() {
        var is = global.parent !== global;
        try {
            is = is && global.parent.document.body.tagName !== 'FRAMESET';
        } catch (e) { }
        return is;
    }

    function sentChangesToIframe(percentage) {
        var iframes = document.querySelectorAll('iframe') || [];
        var frames = document.querySelectorAll('frame') || [];
        for (var i = 0; i < iframes.length; i++) {
            try {
                iframes[i].contentWindow.postMessage(
                    { type: 'changePercentage', percentage: percentage }, '*'
                );
            } catch (e) { }
        }
        for (var j = 0; j < frames.length; j++) {
            try {
                frames[j].contentWindow.postMessage(
                    { type: 'changePercentage', percentage: percentage }, '*'
                );
            } catch (e) { }
        }
    }

    var timerHooker = {
        _intervalIds: {},
        _timeoutIds: {},
        _auoUniqueId: 1,
        __percentage: 1.0,
        _setInterval: global.setInterval,
        _clearInterval: global.clearInterval,
        _clearTimeout: global.clearTimeout,
        _setTimeout: global.setTimeout,
        _Date: global.Date,
        __lastDatetime: new Date().getTime(),
        __lastMDatetime: new Date().getTime(),
        defineProperty: Object.defineProperty,
        _mDate: null,

        genUniqueId: function () {
            return this._auoUniqueId++;
        },

        notifyExec: function (uniqueId) {
            if (uniqueId) {
                var _this = this;
                var timeoutInfos = Object.values(this._timeoutIds).filter(function (info) {
                    return info.uniqueId === uniqueId;
                });
                timeoutInfos.forEach(function (info) {
                    _this._clearTimeout.call(global, info.nowId);
                    delete _this._timeoutIds[info.originId];
                });
            }
        },

        getHookedDateConstructor: function () {
            var timerContext = this;
            return function () {
                if (arguments.length > 0) {
                    Object.defineProperty(this, '_innerDate', {
                        configurable: false,
                        enumerable: false,
                        value: new (Function.prototype.bind.apply(
                            timerContext._Date,
                            [null].concat(Array.prototype.slice.call(arguments))
                        ))(),
                        writable: false
                    });
                    return;
                }
                var now = timerContext._Date.now();
                var passTime = now - timerContext.__lastDatetime;
                var hookPassTime = passTime * (1 / timerContext._percentage);
                Object.defineProperty(this, '_innerDate', {
                    configurable: false,
                    enumerable: false,
                    value: new timerContext._Date(timerContext.__lastMDatetime + hookPassTime),
                    writable: false
                });
            };
        },

        getHookedTimerFunction: function (type, timer) {
            var timerContext = this;
            var property = '_' + type + 'Ids';
            return function () {
                var uniqueId = timerContext.genUniqueId();
                var callback = arguments[0];
                if (typeof callback === 'function') {
                    arguments[0] = function () {
                        var returnValue = callback.apply(this, arguments);
                        timerContext.notifyExec(uniqueId);
                        return returnValue;
                    };
                }
                var originMS = arguments[1];
                arguments[1] *= timerContext._percentage;
                var resultId = timer.apply(global, arguments);
                timerContext[property][resultId] = {
                    args: arguments,
                    originMS: originMS,
                    originId: resultId,
                    nowId: resultId,
                    uniqueId: uniqueId,
                    oldPercentage: timerContext._percentage,
                    exceptNextFireTime: timerContext._Date.now() + originMS
                };
                return resultId;
            };
        },

        redirectNewestId: function (args) {
            var id = args[0];
            if (this._intervalIds[id]) {
                args[0] = this._intervalIds[id].nowId;
                delete this._intervalIds[id];
            }
            if (this._timeoutIds[id]) {
                args[0] = this._timeoutIds[id].nowId;
                delete this._timeoutIds[id];
            }
        },

        percentageChangeHandler: function (percentage) {
            var timerContext = this;
            Object.keys(timerContext._intervalIds).forEach(function (id) {
                var idObj = timerContext._intervalIds[id];
                if (!idObj) return;
                idObj.args[1] = Math.floor((idObj.originMS || 1) * percentage);
                timerContext._clearInterval.call(global, idObj.nowId);
                idObj.nowId = timerContext._setInterval.apply(global, idObj.args);
            });
            Object.keys(timerContext._timeoutIds).forEach(function (id) {
                var idObj = timerContext._timeoutIds[id];
                if (!idObj) return;
                var now = timerContext._Date.now();
                var exceptTime = idObj.exceptNextFireTime;
                var oldPercentage = idObj.oldPercentage;
                var time = exceptTime - now;
                if (time < 0) time = 0;
                var changedTime = Math.floor(percentage / oldPercentage * time);
                idObj.args[1] = changedTime;
                idObj.exceptNextFireTime = now + changedTime;
                idObj.oldPercentage = percentage;
                timerContext._clearTimeout.call(global, idObj.nowId);
                idObj.nowId = timerContext._setTimeout.apply(global, idObj.args);
            });
        },

        hookDefine: function () {
            hookBefore(Object, 'defineProperty', function (m, args) {
                var option = args[2];
                var ele = args[0];
                var key = args[1];
                if (option && ele && ele instanceof Element && typeof key === 'string' && key.indexOf('on') >= 0) {
                    option.configurable = true;
                }
                if (ele instanceof HTMLVideoElement && key === 'playbackRate') {
                    option.configurable = true;
                    args[1] = 'playbackRate_hooked';
                }
            });
            hookBefore(Object, 'defineProperties', function (m, args) {
                var option = args[1];
                var ele = args[0];
                if (ele && ele instanceof Element) {
                    Object.keys(option).forEach(function (key) {
                        var o = option[key];
                        if (typeof key === 'string' && key.indexOf('on') >= 0) {
                            o.configurable = true;
                        }
                        if (ele instanceof HTMLVideoElement && key === 'playbackRate') {
                            o.configurable = true;
                            delete option[key];
                            option['playbackRate_hooked'] = o;
                        }
                    });
                }
            });
        },

        hookShadowRoot: function () {
            var origin = Element.prototype.attachShadow;
            hookAfter(Element.prototype, 'attachShadow', function (m, args, result) {
                extraElements.push(result);
                return result;
            });
            hookedToString(origin, Element.prototype.attachShadow);
        },

        changePlaybackRate: function (ele, rate) {
            delete ele.playbackRate;
            delete ele.playbackRate;
            delete ele.playbackRate;
            ele.playbackRate = rate;
            if (rate !== 1) {
                this.defineProperty.call(Object, ele, 'playbackRate', {
                    configurable: true,
                    get: function () { return 1; },
                    set: function () { }
                });
            }
        },

        changeVideoSpeed: function () {
            var rate = 1 / this._percentage;
            if (rate > 16) rate = 16;
            if (rate < 0.065) rate = 0.065;
            var videos = querySelectorAllDeep(document, 'video') || [];
            for (var i = 0; i < videos.length; i++) {
                this.changePlaybackRate(videos[i], rate);
            }
        },

        applyHooking: function () {
            var _this = this;
            hookReplace(global, 'setInterval', function (setInterval) {
                return _this.getHookedTimerFunction('interval', setInterval);
            });
            hookReplace(global, 'setTimeout', function (setTimeout) {
                return _this.getHookedTimerFunction('timeout', setTimeout);
            });
            hookBefore(global, 'clearInterval', function (method, args) {
                _this.redirectNewestId(args);
            });
            hookBefore(global, 'clearTimeout', function (method, args) {
                _this.redirectNewestId(args);
            });

            var newFunc = this.getHookedDateConstructor();
            hookDateClass(this._Date, newFunc);
            global.Date = newFunc;
            Date.now = function () {
                return new Date().getTime();
            };
            hookedToString(this._Date.now, Date.now);

            var objToString = Object.prototype.toString;
            Object.prototype.toString = function toString() {
                if (this instanceof timerHooker._mDate) {
                    return '[object Date]';
                }
                return objToString.call(this);
            };
            hookedToString(objToString, Object.prototype.toString);
            hookedToString(this._setInterval, setInterval);
            hookedToString(this._setTimeout, setTimeout);
            hookedToString(this._clearInterval, clearInterval);

            this._mDate = global.Date;
            this.hookShadowRoot();
        },

        init: function () {
            var timerContext = this;

            this.hookDefine();
            this.applyHooking();

            Object.defineProperty(timerContext, '_percentage', {
                get: function () {
                    return timerContext.__percentage;
                },
                set: function (percentage) {
                    if (percentage === timerContext.__percentage) return percentage;
                    timerContext.percentageChangeHandler(percentage);
                    timerContext.__percentage = percentage;
                    return percentage;
                }
            });

            if (isInIframe()) {
                global.addEventListener('message', function (e) {
                    var data = e.data;
                    if ((data.type || '') === 'changePercentage') {
                        timerContext.change(data.percentage || 0);
                    }
                });
            }
        },

        change: function (percentage) {
            this.__lastMDatetime = this._mDate.now();
            this.__lastDatetime = this._Date.now();
            this._percentage = percentage;
            this.changeVideoSpeed();
            if (!isInIframe()) {
                sentChangesToIframe(percentage);
            }
        }
    };

    timerHooker.init();

    global.TimerHooker = {
        setRate: function (rate) {
            if (typeof rate !== 'number' || rate <= 0) {
                throw new Error('TimerHooker.setRate: rate must be a positive number, got ' + rate);
            }
            timerHooker.change(1 / rate);
        }
    };

})(window);