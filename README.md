# QuakeAI

Quake A.I. is an on-going research project with the goal of creating challenging games customized for every player. It will be applied AI techniques for analyzing players skills and for deciding how the AI should behave accordingly. For this purpose we have developed a Quake3 mod which consists on a weapon-based match between two players in a map with a great variety of collectable items that they need to take in order to increase their chance of winning. We have chosen this particular shooter game as it provides a fitting environment for designing and implementing AI algorithms. This new AI approach can be broken down in the following main development phases: 

- Modeling phase. We define the data structures necessary to represent a discrete approximation of the virtual world and implement algorithms that create the discrete world via physics simulation.
- Decision-making phase. We implement a runtime decision-making system for AI duels that simultaneously simulates both NPC player actions using heuristics to represent numerically how good or bad these actions are. Each action is executed in parallel through a simulation and once all the simulations have been performed, the decision-making algorithm chooses the most optimal solution for each player. The decision will be updated and synchronized with the implemented AI agent that carries out the decision-making plans.
- Challenging phase. We propose a post-game processing algorithm that analyzes the recorded game actions of the human player using the same AI model and decision-making process. The offline process will numerically evaluate each player’s decision by comparing them with other alternatives. Other player skills such as environment awareness, aiming or reaction time will be also considered and numerically evaluated.

  <img width="886" height="540" alt="image" src="https://github.com/user-attachments/assets/17465539-af77-4254-8f78-b0b6daf9e20b" />
  https://youtu.be/4eBNRxVzoYE

We have managed to run the project with a latest Computer generation, though we don't have a minimum specs requirement. We have written documentation in the wiki section https://github.com/enriquegr84/QuakeAI/wiki that is meant for readers with a technical background in computer science, especially in the artificial intelligence field. Despite all the work we have done so far, this project still has a long road ahead and it is worth-noting that the difficulty increases each step on the way as well as the amount of work. Still we have reached a point in which we could show some potential towards accomplishing our goals. Lets see our current roadmap:
- Find people who are insterested and want to participate in this project.
- Move the project to a professional game engine. The current physics system is not human-playable and it has important issues that makes it no reliable. Additionally, the home-made game engine has limitations that makes it harder to create necessary tools for AI Editing & Analyzing.
- Create better AI Editor/Analysis tools. Display both players visible clusters and predictions in Analysis tool. Be able to change clusters in Editor tool
- Create better prediction system.
- Find solutions for the AI limitations, specially the indecisive agent behavior.
- Introduce Machine Learning.
- Once we have completed a competitive AI agent, lets start the challenging phase.
