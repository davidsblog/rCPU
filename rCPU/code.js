$(function() {
    $("#status").text("");
  
    var totalPoints = 500, updateInterval = 200, windowGap = 40;
    var procdata = [], procplot = [];
    var tempdata = [], tempplot = [];
    var cpu_info_cache = [], temp_data_cache = 0;
    var pi = 0, ti = 0;
  
    function dataToRes(data) {
        var res = [];
        for (i=0; i<data.length; i++) {
            res[i] = [ i, data[i] ];
        }
        return res;
    }
  
    function setProcPlots() {
        for (var n=0; n< procplot.length; n++) {
            procplot[n] = $.plot("#cpu"+n, [ dataToRes(procdata[n]) ], {
                series: {
                    shadowSize: 0,
                    color: 'rgba(0,0,0,0)'
                },
                lines: {
                    fill: true,
                    fillColor:
                    n==0 ? { colors: [ 'rgba(255,64,64,0.6)', 'rgba(255,0,0,0.8)' ] }
                         : { colors: [ 'rgba(64,255,64,0.6)', 'rgba(0,255,0,0.8)' ] }
                },
                grid: {
                    color: "#008000",
                    backgroundColor: "#000000"
                },
                yaxis: {
                    min: -1,
                    max: 101,
                    color: "#808080",
                    tickSize: [25]
                },
                xaxis: {
                    show: false
                }
            });

            var overlay = function(index) {
                return function(plot, cvs) {
                    if (!plot) { return; }
                    var cvsWidth = plot.width() / 2;
                    var text = index == 0 ? 'Total CPU' : 'CPU'+index;
                    cvs.font = "bold 40px Arial";
                    cvs.fillStyle = 'rgba(128,128,128,0.5)';
                    cvs.textAlign = 'center';
                    cvs.fillText(text, cvsWidth, 65);
                    return cvs;
                }
            };
            procplot[n].hooks.drawOverlay.push(overlay(n));
        }
    }
  
    function initProcCharts() {
        $.ajax({
            url: "cpu.api",
            type: "post",
            data: { counter:"0" }
        }).done(function(cpu_info) {
            if (cpu_info.length==2) { cpu_info_cache = [ cpu_info[0] ]; }
            else { cpu_info_cache = cpu_info; }

            for (var n=cpu_info_cache.length-1; n>=0; n--) {
                if (n>0) {
                    $("#cpu0").after("<div style=\"height:2px\">&nbsp;</div><div id=\"cpu" + n +"\" style=\"height:100px\" />");
                }
            
                procdata[n] = new Array(totalPoints);
                for (var i = 0; i < totalPoints-1; i++) {
                    procdata[n][i] = 0;
                }
                procdata[n][totalPoints-1] = cpu_info[n];
            }
            procplot = new Array(cpu_info.length);
            setProcPlots();
        });
    }
  
    function updateProcGraphs(cpuData) {
        $("#cpuinfo").text(cpuData[0]+"% ("+(cpuData.length-1)+" CPU)");
        for (var n=cpuData.length-1; n>=0; n--) {
            procdata[n] = procdata[n].slice(1);
            procdata[n].push(cpuData[n]);
            procplot[n].setData([dataToRes(procdata[n])]);
            procplot[n].draw();
        }
    }
  
    function updateProc() {
        if (pi++ % 5 == 0) {
            $.ajax({
                url: "cpu.api",
                type: "post",
                data: { counter:"0" }
            }).done(function(cpu_info) {
                if (cpu_info.length==2) { cpu_info_cache = [ cpu_info[0] ]; }
                else { cpu_info_cache = cpu_info; }
                updateProcGraphs(cpu_info_cache);
            });
        }
        else {
            updateProcGraphs(cpu_info_cache);
        }
    }
  
    function setTempPlot() {
        tempplot = $.plot("#tempChart", [ dataToRes(tempdata) ], {
            series: {
                shadowSize: 0,
                color: 'rgba(0,0,0,0)'
            },
            lines: {
                fill: true,
                fillColor: { colors: [ 'rgba(160,160,255,0.8)', 'rgba(255,0,0,0.8)' ] }
            },
            grid: {
                color: "#6B8E23",
                backgroundColor: "#000000"
            },
            yaxis: {
                min: 20,
                max: 90,
                color: "#808080",
                tickSize: [10]
            },
            xaxis: {
                show: false
            }
        });

        tempplot.hooks.drawOverlay.push(function(plot, cvs) {
            if (!plot) { return; }
            var cvsWidth = plot.width() / 2;
            var text = 'Temperature';
            cvs.font = "bold 40px Arial";
            cvs.fillStyle = 'rgba(128,128,128,0.5)';
            cvs.textAlign = 'center';
            cvs.fillText(text, cvsWidth, 65);
            return cvs;
        });
    }
  
    function initTempChart() {
        $.ajax({
            url: "temp.api",
            type: "post",
        }).done(function(data) {
            if (data!=="?") {
                temp_data_cache = data;
                $("#temp").text(temp_data_cache + "\u00B0C");
                tempdata = new Array(totalPoints);
                for (var i = 0; i < totalPoints-1; i++) {
                    tempdata[i] = 0;
                }
                tempdata[totalPoints-1] = temp_data_cache;
                setTempPlot();
            }
            else
            {
                $("#temp").text("");
                $("#tempChart").hide();
                $("#tempLabel").hide();
            }
        });
    }
  
    function updateTempGraph(data) {
        $("#temp").text(temp_data_cache + "\u00B0C");
        tempdata = tempdata.slice(1);
        tempdata.push(temp_data_cache);
        tempplot.setData([dataToRes(tempdata)]);
        tempplot.draw();
    }
  
    function updateTemp() {
        if (ti++ % 20 == 0) {
            $.ajax({
                url: "temp.api",
                type: "post",
            }).done(function(data) {
                if (data!=="?") {
                    temp_data_cache = data;
                    updateTempGraph(temp_data_cache);
                }
            });
        }
        else {
            updateTempGraph(temp_data_cache);
        }
    }
  
    $(window).bind("resize", function() {
        setProcPlots();
        setTempPlot();
    });
  
    function showUpdates() {
        updateProc();
        updateTemp();
    }
  
    initTempChart();
    initProcCharts();
    setInterval(showUpdates, updateInterval);
});
