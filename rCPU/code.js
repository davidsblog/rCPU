var cpuDataSets = [];
var tempTimeLine = new TimeSeries();
var windowGap = 40;

$(function() {
	$("#status").text("");
    document.getElementById('tempChart').width  = window.innerWidth-windowGap;
    document.getElementById('cpu0').width  = window.innerWidth-windowGap;
	get_temp();
	get_cpu_use();
	setInterval("get_cpu_use()", 1000);
	setInterval("get_temp()", 5000);
	var timeline = new SmoothieChart(
  	{
		millisPerPixel: 80,
     		grid:
     		{
        		strokeStyle: '#555555',
        		verticalSections: 4 
     		}
  	});
  	timeline.addTimeSeries(tempTimeLine,
    {
        strokeStyle: 'rgba(0, 255, 0, 1)',
        fillStyle: 'rgba(0, 255, 0, 0.2)',
        lineWidth: 3
    });
  	timeline.streamTo(document.getElementById('tempChart'), 1000);
});

function get_cpu_use()
{
   $.ajax({
		url: "cpu.api",
		type: "post",
		data: { counter:"0" }
	}).done(function(cpu_info)
	{
    	$("#cpuinfo").text(cpu_info[0]+"% ("+(cpu_info.length-1)+" CPU)");       
     	var needs_init = 0;
    	if (cpuDataSets.length==0)
    	{
        	needs_init = 1;
        	for (var n=cpu_info.length-1; n>=0; n--)
            {
                cpuDataSets.push(new TimeSeries());
                if (n>0)
                {
                    $("#cpu0").after("<div style=\"height:2px\">&nbsp;</div><canvas id=\"cpu" + n +"\" height=\"100\" />");
                    document.getElementById('cpu'+n).width  = window.innerWidth-windowGap;
                }
            }
    	}
    	for (var n=0; n<cpu_info.length; n++)
    	{
    		cpuDataSets[n].append(new Date().getTime(), cpu_info[n]);
        	if (needs_init == 1)
            {
        		initChart(n);
        	}
      	}
        if (needs_init == 1)
        {
            $(window).resize(function()
            {
                document.getElementById('tempChart').width  = window.innerWidth-windowGap;
                for (var n=0; n<cpu_info.length; n++)
                {
                    document.getElementById('cpu'+n).width  = window.innerWidth-windowGap;
                }
            });
        }
	});
}

function get_temp()
{
	$.ajax({
        	url: "temp.api",
        	type: "post",
    }).done(function(data)
    {
            if (data!=="?")
            {
                $("#temp").text(data + "\u00B0C");
                tempTimeLine.append(new Date().getTime(), data);
            }
            else
            {
                $("#temp").text("");
                $("#tempChart").hide();
                $("#tempLabel").hide();
                clearInterval(tempInterval);
            }
	});
}

function initChart(cpuId)
{
  var seriesOptions;
  if (cpuId==0)
  {
    seriesOptions =
    { 
      strokeStyle: 'rgba(255, 0, 0, 1)',
      fillStyle: 'rgba(255, 0, 0, 0.2)',
      lineWidth: 3
    };
  }
  else
  {
    seriesOptions =
    { 
     strokeStyle: 'rgba(0, 0, 255, 1)',
     fillStyle: 'rgba(0, 0, 255, 0.2)',
     lineWidth: 2
    };
  }
  var timeline = new SmoothieChart(
  {
     labels: {disabled: true},
     maxValue: 103,
     minValue: -3,
     millisPerPixel: 80,
     grid:
     {
        strokeStyle: '#555555',
        lineWidth: 1,
        millisPerLine: 1000,
        verticalSections: 4 
     }
  });
  timeline.addTimeSeries(cpuDataSets[cpuId], seriesOptions);
  timeline.streamTo(document.getElementById('cpu'+cpuId), 1000);
}
