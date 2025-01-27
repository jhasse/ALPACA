#include "game.hpp"
#include "interactable_object.hpp"

std::optional<jngl::Vec2> getPointPosition(std::shared_ptr<Game> game, const std::string &pointName)
{
	std::optional<jngl::Vec2> position;
	// first look for the point in the "this" object,
	// if there is no such point, look in the current scene background
	std::shared_ptr<SpineObject> obj = (*game->lua_state)["this"];
	position = obj->getPoint(pointName);
	if (!position)
	{
		obj = game->player;
		position = obj->getPoint(pointName);
	}
	if (!position)
	{
		obj = game->currentScene->background;
		position = obj->getPoint(pointName);
	}
	return position;
}

void Game::setupLuaFunctions()
{
	/// pass is a function doing nothing
	/// You can use it for testing or for not needed callbacks
	lua_state->set_function("pass",
							[]()
							{
							});

	/// Loads a new Scene/Room
	///
	/// Door Example expects a Spine point object near the door:
	///
	///function door()
	///	LoadScene("cockpit")
	///end
	///GoToPoint("cockpit", door)
	lua_state->set_function("LoadScene",
							[this](const std::string &scene)
							{
								loadLevel(scene);
							});

	/// Prevent the player to skip a interaktion and walking somewere else
	lua_state->set_function("InterruptibleOff",
							[this]()
							{
								player->interruptible = false;
							});

	/// Enable interruption again
	lua_state->set_function("InterruptibleOn",
							[this]()
							{
								player->interruptible = true;
							});

	/// Plays immediately an animation on the calling Spine object
	/// int trackIndex: Spines animation track.
	/// string newAnimation: Animation name that will be played.
	/// bool loop: Should the animation be looped at the end.
	/// function callback: called on the end of the animation, also on looped animations.
	lua_state->set_function("PlayAnimation",
							[this](int trackIndex, const std::string &newAnimation, bool loop, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->playAnimation(trackIndex, newAnimation, loop, callback);
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".animation = \"" + newAnimation + "\"");
								if (loop)
								{
									lua_state->script(lua_object + ".loop_animation = true");
								}
								else
								{
									lua_state->script(lua_object + ".loop_animation = false");
								}
							});

	/// Adds an animation on the calling Spine object that will be played after the current animation ends
	/// int trackIndex: Spines animation track.
	/// string newAnimation: Animation name that will be played.
	/// bool loop: Should the animation be looped at the end.
	/// function callback: called on the end of the animation, also on looped animations.
	lua_state->set_function("AddAnimation",
							[this](int trackIndex, const std::string &newAnimation, bool loop, float delay, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->addAnimation(trackIndex, newAnimation, loop, delay, callback);
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".animation = \"" + newAnimation + "\"");
								if (loop)
								{
									lua_state->script(lua_object + ".loop_animation = true");
								}
								else
								{
									lua_state->script(lua_object + ".loop_animation = false");
								}
							});

	/// See PlayAnimation
	/// string object: Objects ID that should be effected
	lua_state->set_function("PlayAnimationOn",
							[this](const std::string &object, int trackIndex, const std::string &newAnimation, bool loop, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->playAnimation(trackIndex, newAnimation, loop, callback);
									std::string lua_object = getLUAPath(obj->getId());
									lua_state->script(lua_object + ".animation = \"" + newAnimation + "\"");
									if (loop)
									{
										lua_state->script(lua_object + ".loop_animation = true");
									}
									else
									{
										lua_state->script(lua_object + ".loop_animation = false");
									}
								}
							});

	/// See AddAnimation
	/// string object: Objects ID that should be effected
	lua_state->set_function("AddAnimationOn",
							[this](const std::string &object, int trackIndex, const std::string &newAnimation, bool loop, float delay, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->addAnimation(trackIndex, newAnimation, loop, delay, callback);
									std::string lua_object = getLUAPath(obj->getId());
									lua_state->script(lua_object + ".animation = \"" + newAnimation + "\"");
									if (loop)
									{
										lua_state->script(lua_object + ".loop_animation = true");
									}
									else
									{
										lua_state->script(lua_object + ".loop_animation = false");
									}
								}
							});

	/// Set Spine skin on an Spine object
	/// string skin: Spines Skin name
	lua_state->set_function("SetSkin",
							[this](const std::string &skin)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								jngl::debugLn("setting skin of " + obj->getName() + " to " + skin);
								obj->setSkin(skin);
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".skin = \"" + skin + "\"");
							});

	/// See SetSkin
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetSkinOn",
							[this](const std::string &object, const std::string &skin)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setSkin(skin);
									std::string lua_object = getLUAPath(object);
									lua_state->script(lua_object + ".skin = \"" + skin + "\"");
								}
							});

	/// Plays a dialog by name
	/// string dialogName: Name of the skin that will be set.
	lua_state->set_function("PlayDialog",
							[this](const std::string &dialogName, sol::function callback)
							{
								float x = 0;
								float y = 0;
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								auto slot = spSkeleton_findSlot(obj->skeleton->skeleton, "head");
								if (slot)
								{
									auto att = spSkeleton_getAttachmentForSlotName((obj)->skeleton->skeleton, "head", "head");
									spPointAttachment *point = SUB_CAST(spPointAttachment, att);
									spPointAttachment_computeWorldPosition(point, slot->bone, &x, &y);
								}
								getDialogManager()->play(dialogName, jngl::Vec2(x, -y) + obj->getPosition(), callback);
							});

	/// Adds the current item to the inventory.
	/// The items skin will be set to inventar_default_skin
	/// The item will be moved from the scene to inventory_items
	lua_state->set_function("AddToInventory",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setSkin(config["inventar_default_skin"].as<std::string>());
								obj->cross_scene = true;
								obj->setVisible(false);
								lua_state->script("inventory_items[\"" + obj->getId() + "\"] = scenes[game.scene].items." + obj->getId());
								lua_state->script("inventory_items[\"" + obj->getId() + "\"].skin = \"" + config["inventar_default_skin"].as<std::string>() + "\"");
								lua_state->script("inventory_items[\"" + obj->getId() + "\"].cross_scene = true");

								lua_state->script("scenes[game.scene].items." + obj->getId() + " = nil");
							});

	/// See AddToInventory
	/// string skin: A skin that will be set insted of inventar_default_skin
	lua_state->set_function("AddToInventoryWithSkin",
							[this](const std::string &skin)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setSkin(skin);
								obj->cross_scene = true;
								obj->setVisible(false);
								lua_state->script("inventory_items[\"" + obj->getId() + "\"] = scenes[game.scene].items." + obj->getId());
								lua_state->script("inventory_items[\"" + obj->getId() + "\"].skin = \"" + skin + "\"");
								lua_state->script("inventory_items[\"" + obj->getId() + "\"].cross_scene = true");

								lua_state->script("scenes[game.scene].items." + obj->getId() + " = nil");
							});

	/// See AddToInventory
	/// string object: Objects ID that should be effected
	/// Note: The object has to be in the scene, this function does not create new objects. The object can be placed outside of the scenes view.
	lua_state->set_function("AddToInventoryOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setSkin(config["inventar_default_skin"].as<std::string>());
									obj->cross_scene = true;
									obj->setVisible(false);
									lua_state->script("inventory_items[\"" + object + "\"] = scenes[game.scene].items." + object);
									lua_state->script("inventory_items[\"" + object + "\"].skin = \"" + config["inventar_default_skin"].as<std::string>() + "\"");
									lua_state->script("inventory_items[\"" + object + "\"].cross_scene = true");

									lua_state->script("scenes[game.scene].items." + object + " = nil");
								}
							});

	/// See AddToInventory
	/// string skin: A skin that will be set insted of inventar_default_skin
	/// string object: Objects ID that should be effected
	/// Note: See AddToInventoryOn's note.
	lua_state->set_function("AddToInventoryWithSkinOn",
							[this](const std::string &object, const std::string &skin)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setSkin(config[skin].as<std::string>());
									obj->cross_scene = true;
									obj->setVisible(false);
									lua_state->script("inventory_items[\"" + object + "\"] = scenes[game.scene].items." + object);
									lua_state->script("inventory_items[\"" + object + "\"].skin = \"" + skin + "\"");
									lua_state->script("inventory_items[\"" + object + "\"].cross_scene = true");

									lua_state->script("scenes[game.scene].items." + object + " = nil");
								}
							});

	/// DEPRECATED use SetDeleted
	lua_state->set_function("RemoveFromInventory",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								lua_state->script("inventory_items[\"" + obj->getId() + "\"] = nil");
								obj->cross_scene = false;
							});

	/// DEPRECATED use SetDeleted
	lua_state->set_function("RemoveFromInventoryOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									lua_state->script("inventory_items[\"" + object + "\"] = nil");
									obj->cross_scene = false;
								}
							});

	/// Set deleted in the current object.
	/// Objects get only deleted at the end of the frame.
	lua_state->set_function("SetDeleted",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								auto inter = std::static_pointer_cast<InteractableObject>(obj);
								if (inter)
									inter->registerToDelete();
							});

	/// See SetDeleted
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetDeletedOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								auto inter = std::static_pointer_cast<InteractableObject>(obj);
								if (inter)
									inter->registerToDelete();
							});

	/// Get all Spine points from this Spine object
	/// returns: a list of positions
	lua_state->set_function("GetPointNames",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								return obj->getPointNames();
							});

	/// string object: Objects ID that should be effected
	/// returns: a list of positions
	/// See GetPointNames
	/// LUA Example:
	///
	/// local points = GetPointNamesOn("inventory_object")
	/// for i = 1, #points do
    ///    print(i, points[i])
	/// end
	lua_state->set_function("GetPointNamesOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									return sol::as_table(obj->getPointNames());
								}
								throw std::runtime_error("No object " + object);
							});

	/// Send the player to a point's position of this Spine object.
	/// string point_name: Name of the point the player should go to
	/// function callback: Function that will becalled when the layer reaches the position
	lua_state->set_function("GoToPoint",
							[this](const std::string &point_name, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								auto position = obj->getPoint(point_name);
								if (!position)
									return;
								std::static_pointer_cast<InteractableObject>(obj)->goToPosition(*position, callback);
								// TODO Write Players position to LUA
							});

	/// See GoToPoint
	/// string object: Objects ID that should be effected
	lua_state->set_function("GoToPointOn",
							[this](const std::string &object, const std::string &point_name, sol::function callback)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									auto position = obj->getPoint(point_name);
									if (!position)
										return;
									std::static_pointer_cast<InteractableObject>(obj)->goToPosition(*position, callback);
									// TODO Write Players position to LUA
								}
							});

	/// Stop player in position
	lua_state->set_function("StopWalking",
							[this]()
							{
								player->stop_walking();
							});

	/// Debug function to get the position of a Spine point.
	/// string point_name: Spine point name
	/// returns: Tupe(x, y)
	lua_state->set_function("GetPointPosition",
							[this](const std::string &point_name)
							{
								auto position = getPointPosition(shared_from_this(), point_name);
								if (position)
									return std::tuple(position->x, position->y);

								return std::tuple(0.0, 0.0);
							});

	/// Set's an objects position to a Spine point
	/// string point_name: Spine point name
	lua_state->set_function("SetPositionToPoint",
							[this](const std::string &point_name)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								auto position = getPointPosition(shared_from_this(), point_name);

								if (position)
								{
									obj->setPosition(position.value());
									std::string lua_object = getLUAPath(obj->getId());
									lua_state->script(lua_object + ".x = \"" + std::to_string(position->x) + "\"");
									lua_state->script(lua_object + ".y = \"" + std::to_string(position->y) + "\"");
								}
							});

	/// See SetPositionToPoint
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetPositionToPointOn",
							[this](const std::string &object, const std::string &point_name)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									std::shared_ptr<SpineObject> self = (*lua_state)["this"];
									auto position = getPointPosition(shared_from_this(), point_name);
									if (position)
									{
										obj->setPosition(position.value());
										std::string lua_object = getLUAPath(object);
										lua_state->script(lua_object + ".x = \"" + std::to_string(position->x) + "\"");
										lua_state->script(lua_object + ".y = \"" + std::to_string(position->y) + "\"");
									}
								}
							});

	// See SetPositionToPoint
	/// string object: Objects ID that should be effected
	/// string from: Objects ID that provides the point
	lua_state->set_function("SetOnToPointFrom",
							[this](const std::string &object, std::string from, const std::string &point_name)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								std::shared_ptr<SpineObject> frm = getObjectById(from);
								if (frm)
								{
									auto position = frm->getPoint(point_name);
									if(position && frm->abs_position)
									{
										position = position.value() + getCameraPosition();
									}

									if (obj && position)
									{
										obj->setPosition(position.value());
										std::string lua_object = getLUAPath(object);
										lua_state->script(lua_object + ".x = \"" + std::to_string(position->x) + "\"");
										lua_state->script(lua_object + ".y = \"" + std::to_string(position->y) + "\"");
									}
								}
							});

	// /// TODO not implemented jet
	// lua_state->set_function("PathLengthToPoint",
	// 						[](const std::string &point_name, sol::function callback)
	// 						{
	// 							// TODO
	// 						});

	// /// TODO not implemented jet
	// lua_state->set_function("SetFont",
	// 						[](const std::string &font_name)
	// 						{
	// 							// TODO
	// 						});

	// /// TODO not implemented jet
	// lua_state->set_function("SaveGame",
	// 						[](const std::string &save_file)
	// 						{
	// 							// TODO
	// 						});

	// /// TODO not implemented jet
	// lua_state->set_function("LoadGame",
	// 						[](const std::string &save_file)
	// 						{
	// 							// TODO
	// 						});

	/// Hides a Spine object.
	lua_state->set_function("SetHidden",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setVisible(false);
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".visible = false");
							});

	/// See SetHidden
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetHiddenOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setVisible(false);
									std::string lua_object = getLUAPath(object);
									lua_state->script(lua_object + ".visible = false");
								}
							});

	/// Unhides a Spine object.
	lua_state->set_function("SetVisible",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setVisible(true);
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".visible = true");
							});

	/// See SetVisible
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetVisibleOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setVisible(true);
									std::string lua_object = getLUAPath(object);
									lua_state->script(lua_object + ".visible = true");
								}
							});

	/// Sets the render layer of an Object. Via layers it's also able to make objects under a specific laxer not interactable.
	/// int layer: Layer number
	lua_state->set_function("SetLayer",
							[this](int layer)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->layer = layer;
								std::string lua_object = getLUAPath(obj->getId());
								lua_state->script(lua_object + ".layer = " + std::to_string(layer));
							});

	/// See SetLayer
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetLayerOn",
							[this](const std::string &object, int layer)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->layer = layer;
									std::string lua_object = getLUAPath(object);
									lua_state->script(lua_object + ".layer = " + std::to_string(layer));
								}
							});

	/// Set all objects with a lower layer this value not interactable. Usefull for a menu, inventory or an intro playing onto of a scene.
	/// int layer: Layer [default: 0]
	lua_state->set_function("SetInactivLayerBorder",
							[this](int layer)
							{
								setInactivLayerBorder(layer);
							});

	/// Attatch the spine object to the mouse pointer
	lua_state->set_function("AttatchToPointer",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setParent(pointer);
								pointer->attatchedObjects.push_back(obj);
							});

	/// See AttatchToPointer
	/// string object: Objects ID that should be effected
	lua_state->set_function("AttatchToPointerOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setParent(pointer);
									pointer->attatchedObjects.push_back(obj);
								}
							});

	/// TODO not implemented jet
	lua_state->set_function("DeattatchAllFromPointer",
							[this]()
							{
								for (auto &obj : pointer->attatchedObjects)
								{
									obj->setParent(nullptr);
									obj->setVisible(false);
								}
								pointer->attatchedObjects.clear();
							});

	/// Deatatch the Spine object from the pointer
	lua_state->set_function("DeattatchFromPointer",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setParent(nullptr);

								for (auto it = pointer->attatchedObjects.begin(); it != pointer->attatchedObjects.end();)
								{
									if ((*it) == nullptr || (*it) == obj)
									{
										it = pointer->attatchedObjects.erase(it);
									}
									else
									{
										++it;
									}
								}
							});

	/// See DeattatchFromPointer
	/// string object: Objects ID that should be effected
	lua_state->set_function("DeattatchFromPointerOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setParent(nullptr);

									for (auto it = pointer->attatchedObjects.begin(); it != pointer->attatchedObjects.end();)
									{
										if ((*it) == nullptr || (*it) == obj)
										{
											it = pointer->attatchedObjects.erase(it);
										}
										else
										{
											++it;
										}
									}
								}
							});

	/// Is the attatched spine object to the mouse pointer
	/// returns: bool
	lua_state->set_function("IsAttatchedToPointer",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								auto parent = obj->getParent();
								return parent != nullptr;
							});

	/// See IsAttatchedToPointer
	/// string object: Objects ID that should be effected
	/// returns: bool
	lua_state->set_function("IsAttatchedToPointerOn",
							[this](const std::string &object)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									auto parent = obj->getParent();
									return parent != nullptr;
								}
								return false;
							});

	/// Is something attatched to the mouse pointer
	/// returns: bool
	lua_state->set_function("IsSomethingAttatchedToPointer",
							[this]()
							{
								return pointer->attatchedObjects.size() != 0;
							});

	/// Rote a Spine object
	/// float rotation: A number between 0.0 and 365.0
	lua_state->set_function("SetRotation",
							[this](float rotation)
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								obj->setRotation(rotation);
							});

	/// See SetRotation
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetRotationOn",
							[this](const std::string &object, float rotation)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								if (obj)
								{
									obj->setRotation(rotation);
								}
							});

	/// Get time in secunds since the start of the game
	/// returns: Number
	lua_state->set_function("GetTime",
							[]()
							{
								return jngl::getTime();
							});

	/// Set players max walking speed. Can be set to 0 if the player should not be able to walk.
	/// float speed: Max speed value
	lua_state->set_function("SetPlayerMaxSpeed",
							[this](float max_speed)
							{
								player->setMaxSpeed(max_speed);
								(*lua_state)["player"]["max_speed"] = max_speed;
							});

	/// Creates a game object from a Spine file
	lua_state->set_function("CreateObject",
							[this](std::string spine_file, std::string id, float scale)
							{
								auto interactable = currentScene->createObject(spine_file, id, scale);

								std::string scene = (*lua_state)["game"]["scene"];
								(*lua_state)["scenes"][scene]["items"][id] = lua_state->create_table_with(
									"spine", spine_file,
									"object", std::static_pointer_cast<SpineObject>(interactable),
									"x", 0,
									"y", 0,
									"animation", config["spine_default_animation"].as<std::string>(),
									"loop_animation", true,
									"visible", true,
									"layer", 1,
									"scale", scale);

								add(std::static_pointer_cast<SpineObject>(interactable));
							});

	/// Play a audio file via script. It's much better to use Spine events to trigger sound to get them in sync with the animation
	/// string file: The audio file
	lua_state->set_function("PlayAudio",
							[](const std::string &file)
							{
								jngl::play("audio/" + file);
							});

	/// Set SpeechBubble to a Spine point
	/// string point_name: Spine point name
	lua_state->set_function("SetSpeechBubbleToPoint",
							[this](const std::string &point_name)
							{
								auto position = getPointPosition(shared_from_this(), point_name);
								if (position)
								{
									getDialogManager()->setSpeechBubblePosition(position.value());
									(*lua_state)["speech_bubble_position_x"] = position->x;
									(*lua_state)["speech_bubble_position_y"] = position->y;
								}
							});

	/// See SetSpeechBubbleToPoint
	/// string object: Objects ID that should be effected
	lua_state->set_function("SetSpeechBubbleToPointOn",
							[this](const std::string &object, const std::string &point_name)
							{
								std::shared_ptr<SpineObject> obj = getObjectById(object);
								auto position = obj->getPoint(point_name);
								if (position)
								{
									getDialogManager()->setSpeechBubblePosition(position.value());
									(*lua_state)["speech_bubble_position_x"] = position->x;
									(*lua_state)["speech_bubble_position_y"] = position->y;
								}
							});

	/// Get the objects ID
	/// returns: the name as string
	lua_state->set_function("GetID",
							[this]()
							{
								std::shared_ptr<SpineObject> obj = (*lua_state)["this"];
								return obj->getId();
							});

	/// Set the players X Scale
	/// float: Scale
	lua_state->set_function("SetPlayerScaleX",
							[this](const float scale)
							{
								player->skeleton->skeleton->scaleX = scale;
							});

	/// Set language
	/// string: example 'de' or 'en'
	lua_state->set_function("SetLanguage",
							[this](const std::string language)
							{
								YAML::Node languages = this->config["supportedLanguages"];
								for (auto supported_language : languages)
								{
									if (language == supported_language.as<std::string>())
									{
										this->language = language;
										std::string dialogFilePath = config["dialog"].as<std::string>();
										getDialogManager()->loadDialogsFromFile(dialogFilePath, false);
										return;
									}
								}
							});
}
